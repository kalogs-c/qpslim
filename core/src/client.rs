use std::fmt;

use crate::proto::{
    DEFAULT_TARGET_FPS, ProtoError, Reply, Request, StatsReply, decode_reply,
    pipe_name,
};

pub trait Transport {
    fn roundtrip(&self, request: &str) -> Result<String, ClientError>;
}

pub struct PipeTransport {
    path: String,
}

impl PipeTransport {
    pub fn new(path: String) -> Self {
        PipeTransport { path }
    }
}

#[cfg(windows)]
impl Transport for PipeTransport {
    fn roundtrip(&self, request: &str) -> Result<String, ClientError> {
        use std::fs::OpenOptions;
        use std::io::{BufRead, BufReader, Write};
        let mut pipe = OpenOptions::new().read(true).write(true).open(&self.path)?;
        pipe.write_all(request.as_bytes())?;
        pipe.write_all(b"\n")?;
        let mut line = String::new();
        BufReader::new(pipe).read_line(&mut line)?;
        if line.ends_with('\n') {
            line.pop();
        }
        if line.ends_with('\r') {
            line.pop();
        }
        Ok(line)
    }
}

#[cfg(not(windows))]
impl Transport for PipeTransport {
    fn roundtrip(&self, _request: &str) -> Result<String, ClientError> {
        Err(ClientError::Io(std::io::Error::new(
            std::io::ErrorKind::Unsupported,
            format!("named-pipe transport needs Windows: {}", self.path),
        )))
    }
}

#[derive(Debug)]
pub enum ClientError {
    Io(std::io::Error),
    Proto(ProtoError),
}

impl fmt::Display for ClientError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ClientError::Io(e) => write!(f, "pipe io: {e}"),
            ClientError::Proto(e) => write!(f, "protocol: {e}"),
        }
    }
}

impl std::error::Error for ClientError {}

impl From<std::io::Error> for ClientError {
    fn from(e: std::io::Error) -> Self {
        ClientError::Io(e)
    }
}

impl From<ProtoError> for ClientError {
    fn from(e: ProtoError) -> Self {
        ClientError::Proto(e)
    }
}

pub struct LimiterClient<T: Transport> {
    transport: T,
    target: u32,
}

impl<T: Transport> LimiterClient<T> {
    pub fn new(transport: T) -> Self {
        LimiterClient {
            transport,
            target: DEFAULT_TARGET_FPS,
        }
    }

    pub fn set_fps(&mut self, fps: u32) -> Result<(), ClientError> {
        let request = Request::set_fps(fps)?;
        self.send(&request.encode())?;
        if fps != 0 {
            self.target = fps;
        }
        Ok(())
    }

    pub fn enable(&mut self, on: bool) -> Result<(), ClientError> {
        let fps = if on { self.target } else { 0 };
        let request = Request::set_fps(fps)?;
        self.send(&request.encode())
    }

    pub fn stats(&self) -> Result<StatsReply, ClientError> {
        let line = self.transport.roundtrip(&Request::GetStats.encode())?;
        match decode_reply(&line)? {
            Reply::Stats(s) => Ok(s),
            Reply::Ok => Err(ClientError::Proto(ProtoError::Malformed)),
        }
    }

    fn send(&self, body: &str) -> Result<(), ClientError> {
        let line = self.transport.roundtrip(body)?;
        match decode_reply(&line)? {
            Reply::Ok => Ok(()),
            Reply::Stats(_) => Err(ClientError::Proto(ProtoError::Malformed)),
        }
    }
}

impl LimiterClient<PipeTransport> {
    pub fn for_pid(pid: u32) -> Self {
        LimiterClient::new(PipeTransport::new(pipe_name(pid)))
    }
}
