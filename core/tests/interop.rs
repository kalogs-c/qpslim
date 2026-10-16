use qpslim_core::proto::{decode_reply, Reply, Request};

const VECTORS: &str = include_str!("../../native/common/testdata/pipe_vectors.txt");

fn vector(name: &str) -> &str {
    for line in VECTORS.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        if let Some((key, json)) = line.split_once('=') {
            if key == name {
                return json;
            }
        }
    }
    panic!("missing vector {name}");
}

#[test]
fn rust_requests_match_shared_vectors() {
    assert_eq!(Request::set_fps(0).unwrap().encode(), vector("set_fps_0"));
    assert_eq!(Request::set_fps(60).unwrap().encode(), vector("set_fps_60"));
    assert_eq!(
        Request::set_fps(1000).unwrap().encode(),
        vector("set_fps_1000")
    );
    assert_eq!(Request::GetStats.encode(), vector("get_stats"));
}

#[test]
fn rust_decodes_native_shared_replies() {
    assert_eq!(decode_reply(vector("ok")), Ok(Reply::Ok));
    for key in ["err_fps", "err_op", "err_version", "err_stats"] {
        assert!(
            decode_reply(vector(key)).is_err(),
            "{key} must decode as remote error"
        );
    }
    match decode_reply(vector("stats")).unwrap() {
        Reply::Stats(s) => {
            assert_eq!(s.present_count, 1234);
            assert!((s.fps_avg - 59.94).abs() < 1e-9);
            assert!((s.frametime_avg_ms - 16.685).abs() < 1e-9);
            assert!((s.frametime_max_ms - 17.031).abs() < 1e-9);
        }
        Reply::Ok => panic!("stats vector must decode as Stats"),
    }
}
