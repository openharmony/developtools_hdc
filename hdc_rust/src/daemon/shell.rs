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
use std::sync::Arc;
use std::time::Duration;

use ylong_runtime::sync::mpsc;
use ylong_runtime::sync::Mutex;

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

    pub fn set_pts(&mut self, pts: &Pts, nohup: bool) -> io::Result<()> {
        if !nohup {
            let pipes = pts.setup_pipes()?;
            self.inner.stdin(pipes.stdin);
            self.inner.stdout(pipes.stdout);
            self.inner.stderr(pipes.stderr);
        }

        unsafe { self.inner.pre_exec(pts.session_leader(nohup)) };
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

    pub fn terminal(&self) {
        unsafe {
            let tpgid = libc::tcgetpgrp(self.inner.as_raw_fd());
            if tpgid > 1 {
                libc::kill(tpgid, libc::SIGINT);
            }
        }
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

    pub fn session_leader(&self, nohup: bool) -> impl FnMut() -> io::Result<()> {
        let fd = self.inner.as_raw_fd();
        move || {
            nix::unistd::setsid()?;
            if nohup {
                unsafe { libc::signal(libc::SIGHUP, libc::SIG_IGN) };
            }
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
    pub session_id: u32,
}

struct PtyProcess {
    pub pty: Pty,
    pub child: Arc<Mutex<Child>>,
    pub pty_fd: i32,
    channel_id: u32,
    nohup_flag: bool,
}

impl PtyProcess {
    fn new(pty: Pty, child: Arc<Mutex<Child>>, channel_id: u32, nohup_flag: bool) -> Self {
        let pty_fd = pty.as_fd().as_raw_fd();
        Self {
            pty,
            child,
            pty_fd,
            channel_id,
            nohup_flag,
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
                Ok(message)
            }
            Err(e) => {
                hdc::warn!("pty read failed: {e:?}");
                Err(e)
            }
        }
    }
}

// hdc shell "/system/bin/uitest start-daemon /data/app/el2/100/base/com.ohos.devicetest/cache/shmf &"
// hdc shell "nohup test.sh &"
// async cmd will ignor stdout and stderr, if you want the output, cmd format is:
// hdc shell "cmd_xxx >/data/local/tmp/log 2>&1 &"
// example:
// hdc shell "nohup /data/local/tmp/test.sh >/data/local/tmp/log 2>&1 &"
// hdc shell "/data/local/tmp/test.sh >/data/local/tmp/log 2>&1 &"
fn init_pty_process(cmd: Option<String>, channel_id: u32) -> io::Result<PtyProcess> {
    let pty = Pty::new()?;
    let pts = pty.get_pts()?;
    pty.resize(24, 80);

    // Command::new(sh) for interactive
    // Command::new(cmd[0]).args(cmd[1..]) for normal
    let mut nohup_flag = false;
    let child = match cmd {
        None => {
            let mut command = Command::new(SHELL_PROG);
            command.set_pts(&pts, false)?;
            command.spawn()?
        }
        Some(mut cmd) => {
            hdc::debug!("input cmd [{}]", cmd);
            cmd = cmd.trim().to_string();
            if cmd.starts_with('"') {
                cmd = match cmd.strip_prefix('"') {
                    Some(cmd_res) => cmd_res.to_string(),
                    None => cmd,
                };
                cmd = match cmd.strip_suffix('"') {
                    Some(cmd_res) => cmd_res.to_string(),
                    None => cmd,
                };
            }
            nohup_flag = cmd.ends_with('&');
            let params = ["-c", cmd.as_str()].to_vec();
            let mut proc = Command::new(SHELL_PROG);
            let command = proc.args(params);
            hdc::debug!("command[{:?}] args[{:?}]", command.inner.get_program(), command.inner.get_args());
            command.set_pts(&pts, nohup_flag)?;
            command.spawn()?
        }
    };
    Ok(PtyProcess::new(pty, Arc::new(Mutex::new(child)), channel_id, nohup_flag))
}

async fn subprocess_task(
    cmd: Option<String>,
    session_id: u32,
    channel_id: u32,
    ret_command: HdcCommand,
    mut rx: mpsc::BoundedReceiver<Vec<u8>>,
) {
    let mut pty_process = match init_pty_process(cmd.clone(), channel_id) {
        Err(e) => {
            panic!("execute cmd [{cmd:?}] fail: {e:?}");
        },
        Ok(pty) => pty
    };

    PtyChildProcessMap::put(channel_id, pty_process.child.clone()).await;
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
            if val[..].contains(&0x4_u8) {
                // ctrl-D: end pty
                hdc::info!("ctrl-D: end pty");
                // first write enter key, then send ctrl-d signal
                pty_process.pty.write_all(&[0xA_u8]).unwrap();
                pty_process.pty.write_all(&[0x4_u8]).unwrap();
                // todo: if command is send (means enter key is send), will hungup
                break;
            } else if val[..].contains(&0x3_u8) {
                // ctrl-C: end process
                hdc::info!("ctrl-C: end process");
                pty_process.pty.terminal();
                continue;
            }
            pty_process.pty.write_all(&val).unwrap();
        }

        {
            let mut child_lock = pty_process.child.lock().await;
            match child_lock.try_wait() {
                Ok(Some(status)) => {
                    hdc::debug!("interactive shell finish a process {status}");
                    // break;
                }
                Ok(None) => {}
                Err(e) => {
                    hdc::error!("interactive shell wait failed: {e:?}");
                    break;
                }
            }
        }
    }

    if !pty_process.nohup_flag {
        let mut child_lock = pty_process.child.lock().await;
        let kill_resut = child_lock.kill();
        hdc::debug!("subprocess_task kill child, result:{:#?}", kill_resut);
        match child_lock.wait() {
            Ok(exit_status) => {
                hdc::debug!("subprocess_task waiting child exit success, status:{:?}.", exit_status);
            }
            Err(e) => {
                hdc::debug!("subprocess_task waiting child exit fail, error:{:?}.", e);
            }
        }
    } else {
        let mut child_lock = pty_process.child.lock().await;
        hdc::debug!("subprocess_task nohup_flag:{} wait before", pty_process.nohup_flag);
        let ret  = child_lock.wait();
        hdc::debug!("subprocess_task nohup_flag:{} wait after: {:#?}", pty_process.nohup_flag, ret);
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
        Self { handle, tx, session_id }
    }
}

type Child_ = Arc<Mutex<Child>>;
type PtyChildProcessMap_ = Arc<Mutex<HashMap<u32, Child_>>>;
pub struct PtyChildProcessMap {}
impl PtyChildProcessMap {
    fn get_instance() -> PtyChildProcessMap_ {
        static mut PTY_CHILD_MAP: Option<PtyChildProcessMap_> = None;
        unsafe {
            PTY_CHILD_MAP
                .get_or_insert_with(|| Arc::new(Mutex::new(HashMap::new())))
                .clone()
        }
    }

    pub async fn get(channel_id: u32) -> Option<Child_> {
        let pty_child_map = Self::get_instance();
        let map = pty_child_map.lock().await;
        if let Some(pty_child) = map.get(&channel_id) {
            return Some(pty_child.clone());
        }
        None
    }

    pub async fn put(channel_id: u32, pty_child: Child_) {
        let pty_child_map = Self::get_instance();
        let mut map = pty_child_map.lock().await;
        map.insert(channel_id, pty_child);
    }

    pub async fn del(channel_id: u32) {
        let pty_child_map = Self::get_instance();
        let mut map = pty_child_map.lock().await;
        map.remove(&channel_id);
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
        let map = pty_map.lock().await;
        if let Some(pty_task) = map.get(&channel_id) {
            return Some(pty_task.clone());
        }
        None
    }

    pub async fn put(channel_id: u32, pty_task: PtyTask) {
        let pty_map = Self::get_instance();
        let mut map = pty_map.lock().await;
        let arc_pty_task = Arc::new(pty_task);
        map.insert(channel_id, arc_pty_task);
    }

    pub async fn del(channel_id: u32) {
        let pty_map = Self::get_instance();
        let mut map = pty_map.lock().await;
        map.remove(&channel_id);
        let file_name = format!("{SHELL_TEMP}-{channel_id}");
        let _ = std::fs::remove_file(file_name);

        PtyChildProcessMap::del(channel_id).await;
    }

    pub async fn clear(session_id: u32) {
        let pty_map = Self::get_instance();
        let mut channel_list = Vec::new();
        {
            let map = pty_map.lock().await;
            for _iter in map.iter() {
                let pty_task = _iter.1;
                if pty_task.session_id == session_id {
                    let channel_id = *_iter.0;
                    channel_list.push(channel_id);
                    if let Some(pty_child) = PtyChildProcessMap::get(channel_id).await {
                        let mut child = pty_child.lock().await;
                        let kill_resut = child.kill();
                        hdc::debug!("kill child, result:{:#?}", kill_resut);
                        match child.wait() {
                            Ok(exit_status) => {
                                hdc::debug!("waiting child exit success, status:{:?}.", exit_status);
                            }
                            Err(e) => {
                                hdc::debug!("waiting child exit fail, error:{:?}.", e);
                            }
                        }
                        PtyChildProcessMap::del(channel_id).await;
                    }
                    hdc::debug!(
                        "Clear tty task, channel_id:{}, session_id: {}.",
                        channel_id,
                        session_id
                    );
                }
            }
        }

        let mut map = pty_map.lock().await;
        for channel_id in channel_list {
            map.remove(&channel_id);
            let file_name = format!("{SHELL_TEMP}-{channel_id}");
            let _ = std::fs::remove_file(file_name);
        }
    }
}
