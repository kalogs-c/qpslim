use std::cell::RefCell;
use std::rc::Rc;

use qpslim_core::client::{ClientError, LimiterClient, Transport};
use qpslim_core::proto::ProtoError;

struct MockTransport {
    sent: Rc<RefCell<Vec<String>>>,
    stats: String,
    fail: Rc<RefCell<Option<String>>>,
}

impl MockTransport {
    fn with_log(sent: Rc<RefCell<Vec<String>>>, fail: Rc<RefCell<Option<String>>>) -> Self {
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
        if request == r#"{"v":1,"op":"get_stats"}"# {
            return Ok(self.stats.clone());
        }
        Ok(r#"{"v":1,"ok":true}"#.to_string())
    }
}

struct IoTransport;

impl Transport for IoTransport {
    fn roundtrip(&self, _request: &str) -> Result<String, ClientError> {
        Err(ClientError::Io(std::io::Error::new(
            std::io::ErrorKind::BrokenPipe,
            "pipe closed",
        )))
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

fn fail_with(fail: &Rc<RefCell<Option<String>>>, reply: &str) {
    *fail.borrow_mut() = Some(reply.to_string());
}

fn off_on_last(
    sent: &Rc<RefCell<Vec<String>>>,
    client: &mut LimiterClient<MockTransport>,
) -> String {
    client.enable(false).unwrap();
    client.enable(true).unwrap();
    last(sent)
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
fn last_set_fps_wins_toggle_restore() {
    let (sent, _, mut client) = harness();
    client.set_fps(60).unwrap();
    client.set_fps(120).unwrap();
    assert_eq!(
        off_on_last(&sent, &mut client),
        r#"{"v":1,"op":"set_fps","fps":120}"#
    );
}

#[test]
fn set_fps_zero_preserves_target() {
    let (sent, _, mut client) = harness();
    client.set_fps(120).unwrap();
    client.set_fps(0).unwrap();
    assert_eq!(last(&sent), r#"{"v":1,"op":"set_fps","fps":0}"#);
    client.enable(true).unwrap();
    assert_eq!(last(&sent), r#"{"v":1,"op":"set_fps","fps":120}"#);
}

#[test]
fn failed_set_fps_preserves_target() {
    let (sent, fail, mut client) = harness();
    client.set_fps(120).unwrap();
    fail_with(&fail, r#"{"v":1,"ok":false,"err":"bad_fps"}"#);
    assert!(client.set_fps(144).is_err());
    *fail.borrow_mut() = None;
    assert_eq!(
        off_on_last(&sent, &mut client),
        r#"{"v":1,"op":"set_fps","fps":120}"#
    );
}

#[test]
fn fresh_client_enables_to_sixty() {
    let (sent, _, mut client) = harness();
    assert_eq!(
        off_on_last(&sent, &mut client),
        r#"{"v":1,"op":"set_fps","fps":60}"#
    );
}

#[test]
fn out_of_range_never_touches_transport() {
    let (sent, _, mut client) = harness();
    let err = client.set_fps(1001).unwrap_err();
    assert!(matches!(err, ClientError::Proto(ProtoError::BadFps)));
    assert!(sent.borrow().is_empty());
    let err = client.set_fps(u32::MAX).unwrap_err();
    assert!(matches!(err, ClientError::Proto(ProtoError::BadFps)));
    assert!(sent.borrow().is_empty());
}

#[test]
fn rejected_fps_preserves_toggle_target() {
    let (sent, _, mut client) = harness();
    client.set_fps(120).unwrap();
    assert!(client.set_fps(1001).is_err());
    assert_eq!(
        off_on_last(&sent, &mut client),
        r#"{"v":1,"op":"set_fps","fps":120}"#
    );
}

#[test]
fn stats_decodes_mock_reply() {
    let (_, _, client) = harness();
    let stats = client.stats().unwrap();
    assert_eq!(stats.present_count, 1234);
    assert!((stats.fps_avg - 59.94).abs() < 1e-9);
    assert!((stats.frametime_avg_ms - 16.685).abs() < 1e-9);
    assert!((stats.frametime_max_ms - 17.031).abs() < 1e-9);
}

#[test]
fn stats_with_ok_reply_is_malformed() {
    let (_, fail, client) = harness();
    fail_with(&fail, r#"{"v":1,"ok":true}"#);
    let err = client.stats().unwrap_err();
    assert!(matches!(err, ClientError::Proto(ProtoError::Malformed)));
}

#[test]
fn set_fps_with_stats_reply_is_malformed() {
    let (_, fail, mut client) = harness();
    fail_with(
        &fail,
        r#"{"v":1,"ok":true,"present_count":1,"fps_avg":60.0,"frametime_avg_ms":16.6,"frametime_max_ms":17.0}"#,
    );
    let err = client.set_fps(60).unwrap_err();
    assert!(matches!(err, ClientError::Proto(ProtoError::Malformed)));
}

#[test]
fn remote_error_surfaces() {
    let (_, fail, mut client) = harness();
    fail_with(&fail, r#"{"v":1,"ok":false,"err":"bad_fps"}"#);
    let err = client.set_fps(60).unwrap_err();
    assert!(matches!(err, ClientError::Proto(ProtoError::Remote(_))));
    assert_eq!(err.to_string(), "protocol: remote error: bad_fps");
}

#[test]
fn malformed_reply_surfaces() {
    let (_, fail, client) = harness();
    fail_with(&fail, "garbage");
    let err = client.stats().unwrap_err();
    assert!(matches!(err, ClientError::Proto(ProtoError::Malformed)));
}

#[test]
fn io_error_propagates_as_io() {
    let client = LimiterClient::new(IoTransport);
    let err = client.stats().unwrap_err();
    assert!(matches!(err, ClientError::Io(_)));
    assert!(err.to_string().starts_with("pipe io: "));
}
