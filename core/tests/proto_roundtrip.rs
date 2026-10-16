use qpslim_core::proto::{
    decode_reply, pipe_name, ProtoError, Reply, Request, DEFAULT_TARGET_FPS, FPS_MAX, MAX_LINE,
    PROTOCOL_VERSION,
};

#[track_caller]
fn expect_err(input: &str, want: ProtoError) {
    assert_eq!(decode_reply(input).unwrap_err(), want);
}

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
    assert_eq!(Request::set_fps(1001).unwrap_err(), ProtoError::BadFps);
    assert_eq!(Request::set_fps(u32::MAX).unwrap_err(), ProtoError::BadFps);
}

#[test]
fn protocol_constants_match_native_side() {
    assert_eq!(PROTOCOL_VERSION, 1);
    assert_eq!(FPS_MAX, 1000);
    assert_eq!(MAX_LINE, 4096);
    assert_eq!(DEFAULT_TARGET_FPS, 60);
}

#[test]
fn ok_reply_decodes() {
    assert_eq!(decode_reply(r#"{"v":1,"ok":true}"#), Ok(Reply::Ok));
}

#[test]
fn ok_reply_ignores_stray_err_field() {
    assert_eq!(
        decode_reply(r#"{"v":1,"ok":true,"err":"bad_fps"}"#),
        Ok(Reply::Ok)
    );
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
fn stats_reply_decodes_native_three_decimal_format() {
    let reply = decode_reply(
        r#"{"v":1,"ok":true,"present_count":1234,"fps_avg":59.940,"frametime_avg_ms":16.685,"frametime_max_ms":17.031}"#,
    )
    .unwrap();
    match reply {
        Reply::Stats(s) => {
            assert_eq!(s.present_count, 1234);
            assert!((s.fps_avg - 59.94).abs() < 1e-9);
        }
        Reply::Ok => panic!("expected stats"),
    }
}

#[test]
fn stats_reply_tolerates_unknown_fields() {
    let reply = decode_reply(
        r#"{"v":1,"ok":true,"present_count":7,"fps_avg":60.0,"frametime_avg_ms":16.6,"frametime_max_ms":17.0,"future":1}"#,
    );
    assert!(matches!(reply, Ok(Reply::Stats(_))));
}

#[test]
fn version_mismatch_reports_bad_version() {
    expect_err(r#"{"v":2,"ok":true}"#, ProtoError::BadVersion);
    expect_err(r#"{"v":0,"ok":true}"#, ProtoError::BadVersion);
}

#[test]
fn partial_stats_reports_malformed() {
    expect_err(
        r#"{"v":1,"ok":true,"present_count":1}"#,
        ProtoError::Malformed,
    );
    expect_err(
        r#"{"v":1,"ok":true,"present_count":1,"fps_avg":60.0,"frametime_avg_ms":16.6}"#,
        ProtoError::Malformed,
    );
    expect_err(
        r#"{"v":1,"ok":true,"fps_avg":60.0,"frametime_avg_ms":16.6,"frametime_max_ms":17.0}"#,
        ProtoError::Malformed,
    );
}

#[test]
fn malformed_json_reports_malformed() {
    expect_err("not json", ProtoError::Malformed);
    expect_err("", ProtoError::Malformed);
    expect_err(r#"{"v":"1","ok":true}"#, ProtoError::Malformed);
    expect_err(
        r#"{"v":1,"ok":true,"present_count":"x","fps_avg":60.0,"frametime_avg_ms":16.6,"frametime_max_ms":17.0}"#,
        ProtoError::Malformed,
    );
}

#[test]
fn line_length_limit_is_byte_exact() {
    expect_err(&"x".repeat(4097), ProtoError::TooLong);
    expect_err(&"x".repeat(4096), ProtoError::Malformed);
}

#[test]
fn remote_errors_report_detail_or_unknown() {
    let err = decode_reply(r#"{"v":1,"ok":false}"#).unwrap_err();
    assert_eq!(err, ProtoError::Remote("unknown".to_string()));
    assert_eq!(err.to_string(), "remote error: unknown");
    let err = decode_reply(r#"{"v":1,"ok":false,"err":"bad_fps"}"#).unwrap_err();
    assert_eq!(err.to_string(), "remote error: bad_fps");
}

#[test]
fn error_display_strings() {
    assert_eq!(ProtoError::TooLong.to_string(), "line exceeds 4096 bytes");
    assert_eq!(ProtoError::Malformed.to_string(), "malformed reply");
    assert_eq!(
        ProtoError::BadVersion.to_string(),
        "unsupported protocol version"
    );
    assert_eq!(ProtoError::BadFps.to_string(), "fps out of range 0..=1000");
}

#[test]
fn pipe_name_format() {
    assert_eq!(pipe_name(1234), r"\\.\pipe\qpslim-1234");
    assert_eq!(pipe_name(0), r"\\.\pipe\qpslim-0");
}
