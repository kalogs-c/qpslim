use std::cell::RefCell;
use std::rc::Rc;

use qpslim_core::client::{ClientError, LimiterClient, Transport};

struct MockTransport {
    sent: Rc<RefCell<Vec<String>>>,
    stats: String,
    fail: Rc<RefCell<Option<String>>>,
}

impl MockTransport {
    fn with_log(
        sent: Rc<RefCell<Vec<String>>>,
        fail: Rc<RefCell<Option<String>>>,
    ) -> Self {
        MockTransport {
            sent,
            stats: r#"{"v":1,"ok":true,"present_count":1234,"fps_avg":59.94,"frametime_avg_ms":16.685,"frametime_max_ms":17.031}"#.to_string(),
            fail,
        }
    }
}

impl Transport for MockTransport {
    fn roundtrip(&self, request: &str) -> Result<String, ClientError> {
        self.sent.borrow_mut().push(request.to_string());
        if let Some(reply) = self.fail.borrow().clone() {
            return Ok(reply);
        }
        if request.contains("get_stats") {
            return Ok(self.stats.clone());
        }
        Ok(r#"{"v":1,"ok":true}"#.to_string())
    }
}

fn harness() -> (
    Rc<RefCell<Vec<String>>>,
    Rc<RefCell<Option<String>>>,
    LimiterClient<MockTransport>,
) {
    let sent = Rc::new(RefCell::new(Vec::new()));
    let fail = Rc::new(RefCell::new(None));
    let client = LimiterClient::new(MockTransport::with_log(sent.clone(), fail.clone()));
    (sent, fail, client)
}

fn last(sent: &Rc<RefCell<Vec<String>>>) -> String {
    sent.borrow().last().cloned().unwrap_or_default()
}

#[test]
fn set_fps_line_bytes_are_exact() {
    let (sent, _, mut client) = harness();
    client.set_fps(120).unwrap();
    assert_eq!(last(&sent), r#"{"v":1,"op":"set_fps","fps":120}"#);
}

#[test]
fn toggle_remembers_last_target() {
    let (sent, _, mut client) = harness();
    client.set_fps(120).unwrap();
    client.enable(false).unwrap();
    assert_eq!(last(&sent), r#"{"v":1,"op":"set_fps","fps":0}"#);
    client.enable(true).unwrap();
    assert_eq!(last(&sent), r#"{"v":1,"op":"set_fps","fps":120}"#);
}

#[test]
fn fresh_client_enables_to_sixty() {
    let (sent, _, mut client) = harness();
    client.enable(false).unwrap();
    client.enable(true).unwrap();
    assert_eq!(last(&sent), r#"{"v":1,"op":"set_fps","fps":60}"#);
}

#[test]
fn out_of_range_never_touches_transport() {
    let (sent, _, mut client) = harness();
    assert!(client.set_fps(1001).is_err());
    assert!(sent.borrow().is_empty());
}

#[test]
fn stats_decodes_mock_reply() {
    let (_, _, client) = harness();
    let stats = client.stats().unwrap();
    assert_eq!(stats.present_count, 1234);
    assert!((stats.fps_avg - 59.94).abs() < 1e-9);
}

#[test]
fn remote_error_surfaces() {
    let (_, fail, mut client) = harness();
    *fail.borrow_mut() =
        Some(r#"{"v":1,"ok":false,"err":"bad_fps"}"#.to_string());
    let err = client.set_fps(60).unwrap_err();
    assert_eq!(err.to_string(), "protocol: remote error: bad_fps");
}

#[test]
fn malformed_reply_surfaces() {
    let (_, fail, client) = harness();
    *fail.borrow_mut() = Some("garbage".to_string());
    assert!(client.stats().is_err());
}
