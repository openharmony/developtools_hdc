/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//! shell
#![allow(missing_docs)]

use crate::utils::hdc_log::*;
use hdc::config::TaskMessage;
use hdc::config::{HdcCommand, SHELL_PROG, SHELL_TEMP};
use hdc::transfer;

use std::collections::HashMap;
use std::io::{self, Error, ErrorKind, Read as _, Write as _};
use std::os::fd::{AsFd, AsRawFd};
use std::os::unix::process::CommandExt;
use std::process::Child;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use ylong_runtime::sync::mpsc;

struct Command {
    inner: std::process::Command,
}

impl Command {
    pub fn new(prog: &str) -> Self {
        Self {
            inner: std::process::Command::new(prog),
        }
    }

    pub fn args(&mut self, args: Vec<&str>) -> &mut Self {
        self.inner.args(args);
        self
    }

    pub fn set_pts(&mut self, pts: &Pts) -> io::Result<()> {
        let pipes = pts.setup_pipes()?;
        self.inner.stdin(pipes.stdin);
        self.inner.stdout(pipes.stdout);
        self.inner.stderr(pipes.stderr);

        unsafe { self.inner.pre_exec(pts.session_leader()) };
        Ok(())
    }

    pub fn spawn(&mut self) -> io::Result<std::process::Child> {
        self.inner.spawn()
    }
}

struct Pty {
    inner: nix::pty::PtyMaster,
}

impl Pty {
    pub fn new() -> io::Result<Self> {
        if let Ok(pty_master) = nix::pty::posix_openpt(
            nix::fcntl::OFlag::O_RDWR | nix::fcntl::OFlag::O_NOCTTY | nix::fcntl::OFlag::O_CLOEXEC,
        ) {
            if nix::pty::grantpt(&pty_master).is_ok() && nix::pty::unlockpt(&pty_master).is_ok() {
                return Ok(Self { inner: pty_master });
            }
        }
        Err(Error::new(ErrorKind::Other, "pty init failed"))
    }

    pub fn resize(&self, ws_row: u16, ws_col: u16) {
        let size = nix::pty::Winsize {
            ws_row,
            ws_col,
            ws_xpixel: 0,
            ws_ypixel: 0,
        };
        let fd = self.inner.as_raw_fd();
        let _ = unsafe { set_term_size(fd, std::ptr::NonNull::from(&size).as_ptr()) }.map(|_| ());
    }

    pub fn get_pts(&self) -> io::Result<Pts> {
        let fd = std::fs::OpenOptions::new()
            .read(true)
            .write(true)
            .open(nix::pty::ptsname_r(&self.inner)?)?
            .into();
        Ok(Pts { inner: fd })
    }
}

impl std::os::fd::AsFd for Pty {
    fn as_fd(&self) -> std::os::fd::BorrowedFd<'_> {
        let raw_fd = self.inner.as_raw_fd();
        unsafe { std::os::fd::BorrowedFd::borrow_raw(raw_fd) }
    }
}

impl io::Read for Pty {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.inner.read(buf)
    }
}

impl io::Write for Pty {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.inner.write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

struct Pts {
    inner: std::os::fd::OwnedFd,
}

struct Pipes {
    stdin: std::process::Stdio,
    stdout: std::process::Stdio,
    stderr: std::process::Stdio,
}

impl Pts {
    pub fn setup_pipes(&self) -> io::Result<Pipes> {
        Ok(Pipes {
            stdin: self.inner.try_clone()?.into(),
            stdout: self.inner.try_clone()?.into(),
            stderr: self.inner.try_clone()?.into(),
        })
    }

    pub fn session_leader(&self) -> impl FnMut() -> io::Result<()> {
        let fd = self.inner.as_raw_fd();
        move || {
            nix::unistd::setsid()?;
            unsafe { set_controlling_terminal(fd, std::ptr::null()) }?;
            Ok(())
        }
    }
}

nix::ioctl_write_ptr_bad!(set_term_size, libc::TIOCSWINSZ, nix::pty::Winsize);

nix::ioctl_write_ptr_bad!(set_controlling_terminal, libc::TIOCSCTTY, libc::c_int);

pub struct PtyTask {
    pub handle: ylong_runtime::task::JoinHandle<()>,
    pub tx: mpsc::BoundedSender<Vec<u8>>,
}

struct PtyProcess {
    pub pty: Pty,
    pub child: Child,
    pub pty_fd: i32,
    channel_id: u32,
}

impl PtyProcess {
    fn new(pty: Pty, child: Child, channel_id: u32) -> Self {
        let pty_fd = pty.as_fd().as_raw_fd();
        Self {
            pty,
            child,
            pty_fd,
            channel_id,
        }
    }

    async fn pty_echo(
        &mut self,
        buf: &mut [u8],
        ret_command: HdcCommand,
    ) -> io::Result<TaskMessage> {
        match self.pty.read(buf) {
            Ok(bytes) => {
                let message = TaskMessage {
                    channel_id: self.channel_id,
                    command: ret_command,
                    payload: buf[..bytes].to_vec(),
                };
                hdc::trace!("read {bytes} bytes from pty");
                Ok(message)
            }
            Err(e) => {
                hdc::warn!("pty read failed: {e:?}");
                Err(e)
            }
        }
    }
}

fn init_pty_process(cmd: Option<String>, channel_id: u32) -> io::Result<PtyProcess> {
    let pty = Pty::new()?;
    let pts = pty.get_pts()?;
    pty.resize(24, 80);

    // Command::new(sh) for interactive
    // Command::new(cmd[0]).args(cmd[1..]) for normal
    let child = match cmd {
        None => {
            let mut command = Command::new(SHELL_PROG);
            command.set_pts(&pts)?;
            command.spawn()?
        }
        Some(cmd) => {
            let trimed = cmd.trim_matches('"');
            let params = ["-c", trimed].to_vec();
            let mut proc = Command::new(SHELL_PROG);
            let command = proc.args(params);
            command.set_pts(&pts)?;
            command.spawn()?
        }
    };
    Ok(PtyProcess::new(pty, child, channel_id))
}

async fn subprocess_task(
    cmd: Option<String>,
    session_id: u32,
    channel_id: u32,
    ret_command: HdcCommand,
    mut rx: mpsc::BoundedReceiver<Vec<u8>>,
) {
    let mut pty_process = init_pty_process(cmd, channel_id).unwrap();
    let mut buf = [0_u8; 30720];

    loop {
        let mut tv = nix::sys::time::TimeVal::new(0, 50000);
        let mut set = nix::sys::select::FdSet::new();
        set.insert(pty_process.pty_fd);

        match nix::sys::select::select(None, Some(&mut set), None, None, Some(&mut tv)) {
            Ok(_) => {
                if set.contains(pty_process.pty_fd) {
                    match pty_process.pty_echo(&mut buf, ret_command).await {
                        Err(_) => break,
                        Ok(message) => transfer::put(session_id, message).await,
                    }
                }
            }
            Err(e) => {
                hdc::error!("select failed: {e:?}");
                break;
            }
        }

        if let Ok(val) = rx.recv_timeout(Duration::from_millis(50)).await {
            pty_process.pty.write_all(&val).unwrap();
            if val[..].contains(&0x4_u8) {
                // ctrl-D: end pty
                hdc::info!("ctrl-D: end pty");
                break;
            }
        }

        match pty_process.child.try_wait() {
            Ok(Some(_)) => {
                hdc::debug!("interactive shell finish a process");
                // break;
            }
            Ok(None) => {}
            Err(e) => {
                hdc::error!("interactive shell wait failed: {e:?}");
                break;
            }
        }
    }
    let message = TaskMessage {
        channel_id,
        command: HdcCommand::KernelChannelClose,
        payload: vec![1],
    };
    transfer::put(session_id, message).await;
    PtyMap::del(channel_id).await;
}

impl PtyTask {
    pub fn new(
        session_id: u32,
        channel_id: u32,
        cmd: Option<String>,
        ret_command: HdcCommand,
    ) -> Self {
        let (tx, rx) = ylong_runtime::sync::mpsc::bounded_channel::<Vec<u8>>(16);
        let handle = ylong_runtime::spawn(subprocess_task(
            cmd,
            session_id,
            channel_id,
            ret_command,
            rx,
        ));
        Self { handle, tx }
    }
}

type PtyMap_ = Arc<Mutex<HashMap<u32, Arc<PtyTask>>>>;
pub struct PtyMap {}
impl PtyMap {
    fn get_instance() -> PtyMap_ {
        static mut PTY_MAP: Option<PtyMap_> = None;
        unsafe {
            PTY_MAP
                .get_or_insert_with(|| Arc::new(Mutex::new(HashMap::new())))
                .clone()
        }
    }

    pub async fn get(channel_id: u32) -> Option<Arc<PtyTask>> {
        let pty_map = Self::get_instance();
        let map = pty_map.lock().unwrap();
        if let Some(pty_task) = map.get(&channel_id) {
            return Some(pty_task.clone());
        }
        None
    }

    pub async fn put(channel_id: u32, pty_task: PtyTask) {
        let pty_map = Self::get_instance();
        let mut map = pty_map.lock().unwrap();
        let arc_pty_task = Arc::new(pty_task);
        map.insert(channel_id, arc_pty_task);
    }

    pub async fn del(channel_id: u32) {
        let pty_map = Self::get_instance();
        let mut map = pty_map.lock().unwrap();
        map.remove(&channel_id);
        let file_name = format!("{SHELL_TEMP}-{channel_id}");
        let _ = std::fs::remove_file(file_name);
    }
}
