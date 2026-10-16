use std::collections::HashMap;
use std::sync::Mutex;

use qpslim_core::client::{ClientError, LimiterClient, PipeTransport};
use qpslim_core::proto::StatsReply;

struct AppState {
    clients: Mutex<HashMap<u32, LimiterClient<PipeTransport>>>,
}

fn with_client<T>(
    state: tauri::State<AppState>,
    pid: u32,
    action: impl FnOnce(&mut LimiterClient<PipeTransport>) -> Result<T, ClientError>,
) -> Result<T, String> {
    let mut clients = state
        .clients
        .lock()
        .map_err(|_| "limiter state lock poisoned".to_string())?;
    let client = clients
        .entry(pid)
        .or_insert_with(|| LimiterClient::for_pid(pid));
    action(client).map_err(|e| e.to_string())
}

#[tauri::command]
fn set_target(state: tauri::State<AppState>, pid: u32, fps: u32) -> Result<(), String> {
    with_client(state, pid, |client| client.set_fps(fps))
}

#[tauri::command]
fn set_enabled(state: tauri::State<AppState>, pid: u32, on: bool) -> Result<(), String> {
    with_client(state, pid, |client| client.enable(on))
}

#[tauri::command]
fn get_stats(state: tauri::State<AppState>, pid: u32) -> Result<StatsReply, String> {
    with_client(state, pid, |client| client.stats())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(AppState {
            clients: Mutex::new(HashMap::new()),
        })
        .invoke_handler(tauri::generate_handler![set_target, set_enabled, get_stats])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

#[cfg(test)]
mod ipc_tests {
    use super::*;
    use serde_json::json;
    use tauri::test::{mock_builder, mock_context, noop_assets, INVOKE_KEY};

    fn webview() -> tauri::WebviewWindow<tauri::test::MockRuntime> {
        let app = mock_builder()
            .manage(AppState {
                clients: Mutex::new(HashMap::new()),
            })
            .invoke_handler(tauri::generate_handler![set_target, set_enabled, get_stats])
            .build(mock_context(noop_assets()))
            .expect("failed to build mock app");
        tauri::WebviewWindowBuilder::new(&app, "main", Default::default())
            .build()
            .expect("failed to build mock webview")
    }

    fn request(cmd: &str, body: serde_json::Value) -> tauri::webview::InvokeRequest {
        tauri::webview::InvokeRequest {
            cmd: cmd.into(),
            callback: tauri::ipc::CallbackFn(0),
            error: tauri::ipc::CallbackFn(1),
            url: if cfg!(any(windows, target_os = "android")) {
                "http://tauri.localhost"
            } else {
                "tauri://localhost"
            }
            .parse()
            .unwrap(),
            body: tauri::ipc::InvokeBody::Json(body),
            headers: Default::default(),
            invoke_key: INVOKE_KEY.to_string(),
        }
    }

    #[test]
    fn set_target_without_pipe_reports_pipe_error() {
        let window = webview();
        let response = tauri::test::get_ipc_response(
            &window,
            request("set_target", json!({ "pid": 424242, "fps": 60 })),
        );
        match response {
            Err(value) => assert!(
                value.to_string().contains("pipe"),
                "expected a pipe error, got {value}"
            ),
            Ok(_) => panic!("expected a pipe error, no test pipe should exist"),
        }
    }

    #[test]
    fn missing_arg_is_rejected_before_the_handler() {
        let window = webview();
        let response =
            tauri::test::get_ipc_response(&window, request("set_target", json!({ "pid": 424242 })));
        assert!(response.is_err(), "expected arg rejection, got {response:?}");
    }
}
