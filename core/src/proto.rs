use serde::Deserialize;
use std::fmt;

pub const PROTOCOL_VERSION: u32 = 1;
pub const FPS_MAX: u32 = 1000;
pub const DEFAULT_TARGET_FPS: u32 = 60;
pub const MAX_LINE: usize = 4096;

#[derive(Debug, PartialEq)]
pub enum Request {
    SetFps(u32),
    GetStats,
}

impl Request {
    pub fn set_fps(fps: u32) -> Result<Self, ProtoError> {
        if fps > FPS_MAX {
            return Err(ProtoError::BadFps);
        }
        Ok(Request::SetFps(fps))
    }

    pub fn encode(&self) -> String {
        match self {
            Request::SetFps(fps) => format!(
                "{{\"v\":{},\"op\":\"set_fps\",\"fps\":{fps}}}",
                PROTOCOL_VERSION
            ),
            Request::GetStats => format!(
                "{{\"v\":{},\"op\":\"get_stats\"}}",
                PROTOCOL_VERSION
            ),
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct StatsReply {
    pub present_count: u64,
    pub fps_avg: f64,
    pub frametime_avg_ms: f64,
    pub frametime_max_ms: f64,
}

#[derive(Debug, PartialEq)]
pub enum Reply {
    Ok,
    Stats(StatsReply),
}

#[derive(Debug, PartialEq)]
pub enum ProtoError {
    TooLong,
    Malformed,
    BadVersion,
    BadFps,
    Remote(String),
}

impl fmt::Display for ProtoError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ProtoError::TooLong => write!(f, "line exceeds {MAX_LINE} bytes"),
            ProtoError::Malformed => write!(f, "malformed reply"),
            ProtoError::BadVersion => write!(f, "unsupported protocol version"),
            ProtoError::BadFps => write!(f, "fps out of range 0..={FPS_MAX}"),
            ProtoError::Remote(e) => write!(f, "remote error: {e}"),
        }
    }
}

impl std::error::Error for ProtoError {}

#[derive(Deserialize)]
struct RawReply {
    v: u32,
    ok: bool,
    err: Option<String>,
    present_count: Option<u64>,
    fps_avg: Option<f64>,
    frametime_avg_ms: Option<f64>,
    frametime_max_ms: Option<f64>,
}

pub fn decode_reply(line: &str) -> Result<Reply, ProtoError> {
    if line.len() > MAX_LINE {
        return Err(ProtoError::TooLong);
    }
    let raw: RawReply = serde_json::from_str(line).map_err(|_| ProtoError::Malformed)?;
    if raw.v != PROTOCOL_VERSION {
        return Err(ProtoError::BadVersion);
    }
    if !raw.ok {
        return Err(ProtoError::Remote(
            raw.err.unwrap_or_else(|| "unknown".to_string()),
        ));
    }
    match (
        raw.present_count,
        raw.fps_avg,
        raw.frametime_avg_ms,
        raw.frametime_max_ms,
    ) {
        (Some(present_count), Some(fps_avg), Some(frametime_avg_ms), Some(frametime_max_ms)) => {
            Ok(Reply::Stats(StatsReply {
                present_count,
                fps_avg,
                frametime_avg_ms,
                frametime_max_ms,
            }))
        }
        (None, None, None, None) => Ok(Reply::Ok),
        _ => Err(ProtoError::Malformed),
    }
}

pub fn pipe_name(pid: u32) -> String {
    format!(r"\\.\pipe\qpslim-{pid}")
}
