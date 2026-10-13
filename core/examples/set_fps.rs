use std::process::ExitCode;

use qpslim_core::client::LimiterClient;

fn usage() -> ExitCode {
    eprintln!("usage: set_fps <pid> <fps|on|off|stats>");
    ExitCode::from(1)
}

fn run(pid: u32, op: &str) -> Result<(), String> {
    let mut client = LimiterClient::for_pid(pid);
    match op {
        "on" => client.enable(true).map_err(|e| e.to_string()),
        "off" => client.enable(false).map_err(|e| e.to_string()),
        "stats" => {
            let s = client.stats().map_err(|e| e.to_string())?;
            println!(
                "present_count={} fps_avg={:.2} avg_ms={:.3} max_ms={:.3}",
                s.present_count, s.fps_avg, s.frametime_avg_ms, s.frametime_max_ms
            );
            Ok(())
        }
        fps => {
            let target: u32 = fps.parse().map_err(|_| format!("bad fps '{fps}'"))?;
            client.set_fps(target).map_err(|e| e.to_string())?;
            println!("ok target={target}");
            Ok(())
        }
    }
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        return usage();
    }
    let Ok(pid) = args[1].parse() else {
        return usage();
    };
    match run(pid, args[2].as_str()) {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("Error: {e}");
            ExitCode::from(1)
        }
    }
}
