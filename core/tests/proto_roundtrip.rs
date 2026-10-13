use qpslim_core::proto::{
    FPS_MAX, PROTOCOL_VERSION, Reply, Request, decode_reply, pipe_name,
};

#[test]
fn set_fps_encodes_to_golden_bytes() {
    let req = Request::set_fps(60).unwrap();
    assert_eq!(req.encode(), r#"{"v":1,"op":"set_fps","fps":60}"#);
}

#[test]
fn get_stats_encodes_to_golden_bytes() {
    assert_eq!(Request::GetStats.encode(), r#"{"v":1,"op":"get_stats"}"#);
}

#[test]
fn fps_bounds_match_native_arg_limit() {
    assert_eq!(FPS_MAX, 1000);
    assert!(Request::set_fps(0).is_ok());
    assert!(Request::set_fps(1000).is_ok());
    assert!(Request::set_fps(1001).is_err());
}

#[test]
fn ok_reply_decodes() {
    assert_eq!(decode_reply(r#"{"v":1,"ok":true}"#), Ok(Reply::Ok));
}

#[test]
fn stats_reply_decodes() {
    let reply = decode_reply(
        r#"{"v":1,"ok":true,"present_count":1234,"fps_avg":59.94,"frametime_avg_ms":16.685,"frametime_max_ms":17.031}"#,
    )
    .unwrap();
    match reply {
        Reply::Stats(s) => {
            assert_eq!(s.present_count, 1234);
            assert!((s.fps_avg - 59.94).abs() < 1e-9);
            assert!((s.frametime_avg_ms - 16.685).abs() < 1e-9);
            assert!((s.frametime_max_ms - 17.031).abs() < 1e-9);
        }
        Reply::Ok => panic!("expected stats"),
    }
}

#[test]
fn stats_reply_tolerates_unknown_fields() {
    let reply = decode_reply(r#"{"v":1,"ok":true,"present_count":7,"fps_avg":60.0,"frametime_avg_ms":16.6,"frametime_max_ms":17.0,"future":1}"#);
    assert!(matches!(reply, Ok(Reply::Stats(_))));
}

#[test]
fn rejection_matrix() {
    assert!(decode_reply(r#"{"v":2,"ok":true}"#).is_err());
    assert!(decode_reply(r#"{"v":1,"ok":true,"present_count":1}"#).is_err());
    assert!(decode_reply("not json").is_err());
    assert!(decode_reply("").is_err());
    assert!(decode_reply(&"x".repeat(4097)).is_err());
    let err = decode_reply(r#"{"v":1,"ok":false,"err":"bad_fps"}"#).unwrap_err();
    assert_eq!(err.to_string(), "remote error: bad_fps");
    assert_eq!(PROTOCOL_VERSION, 1);
}

#[test]
fn pipe_name_format() {
    assert_eq!(pipe_name(1234), r"\\.\pipe\qpslim-1234");
}
