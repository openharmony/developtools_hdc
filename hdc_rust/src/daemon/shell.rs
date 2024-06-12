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

#[allow(unused_imports)]
use super::task_manager;
use crate::utils::hdc_log::*;
use hdc::config::TaskMessage;
use hdc::config::{HdcCommand, MessageLevel, SHELL_PROG};
use hdc::transfer;
use std::collections::HashMap;
use std::io::{self};
use std::sync::Arc;

use std::os::fd::AsRawFd;
use std::process::Stdio;
use ylong_runtime::io::AsyncReadExt;
use ylong_runtime::io::AsyncWriteExt;
use ylong_runtime::process::pty_process::{Pty, PtyCommand};
use ylong_runtime::process::Child as ylongChild;
use ylong_runtime::sync::mpsc;
use ylong_runtime::sync::Mutex;

pub struct PtyTask {
    pub handle: ylong_runtime::task::JoinHandle<()>,
    pub tx: mpsc::BoundedSender<Vec<u8>>,
    pub session_id: u32,
    pub channel_id: u32,
    pub cmd: Option<String>,
}

struct PtyProcess {
    pub pty: Pty,
    pub child: Arc<Mutex<ylongChild>>,
    pub nohup_flag: bool,
}

impl PtyProcess {
    fn new(pty: Pty, child: Arc<Mutex<ylongChild>>, nohup_flag: bool) -> Self {
        Self {
            pty,
            child,
            nohup_flag,
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
fn init_pty_process(cmd: Option<String>, _channel_id: u32) -> io::Result<PtyProcess> {
    let pty = match Pty::new() {
        Ok(pty) => pty,
        Err(e) => {
            hdc::error!("pty create error: {}", e);
            return Err(e);
        }
    };

    let pts = match pty.pts() {
        Ok(pts) => pts,
        Err(e) => {
            hdc::error!("pty pts error: {}", e);
            return Err(e);
        }
    };
    let mut nohup_flag = false;
    let child = match cmd {
        None => {
            hdc::debug!("input cmd is None. channel_id {_channel_id}");
            let mut command = PtyCommand::new(SHELL_PROG);
            command.spawn(&pts)?
        }
        Some(mut cmd) => {
            hdc::debug!("input cmd [{}]", cmd);
            cmd = cmd.trim().to_string();
            if cmd.starts_with('"') && cmd.ends_with('"') {
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
            let mut proc = PtyCommand::new(SHELL_PROG);
            let command = proc.args(params);
            if nohup_flag {
                unsafe {
                    command.pre_exec(move || {
                        if nohup_flag {
                            libc::setsid();
                            libc::signal(libc::SIGHUP, libc::SIG_IGN);
                        }
                        Ok(())
                    });
                }
                command
                    .stdin(Stdio::null())
                    .stdout(Stdio::null())
                    .stderr(Stdio::null())
                    .spawn(&pts)?
            } else {
                command.spawn(&pts)?
            }
        }
    };
    Ok(PtyProcess::new(
        pty,
        Arc::new(Mutex::new(child)),
        nohup_flag,
    ))
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
            hdc::common::hdctransfer::echo_client(
                session_id,
                channel_id,
                "execute cmd fail".as_bytes().to_vec(),
                MessageLevel::Fail,
            )
            .await;
            let task_message = TaskMessage {
                channel_id,
                command: HdcCommand::KernelChannelClose,
                payload: [1].to_vec(),
            };
            transfer::put(session_id, task_message).await;
            let msg = format!("execute cmd [{cmd:?}] fail: {e:?}");
            hdc::error!("{}", msg);
            panic!("{}", msg);
        }
        Ok(pty) => pty,
    };
    PtyChildProcessMap::put(session_id, channel_id, pty_process.child.clone()).await;
    let mut buf = [0_u8; 30720];
    loop {
        ylong_runtime::select! {
            read_res = pty_process.pty.read(&mut buf) => {
                match read_res {
                    Ok(bytes) => {
                        let message = TaskMessage {
                            channel_id,
                            command: ret_command,
                            payload: buf[..bytes].to_vec(),
                        };
                        transfer::put(session_id, message).await;
                    }
                    Err(e) => {
                        hdc::warn!("pty read failed: {e:?}");
                        break;
                    }
                }
            },
            recv_res = rx.recv() => {
                match recv_res {
                    Ok(val) => {
                        if val[..].contains(&0x4_u8) {
                            // ctrl-D: end pty
                            hdc::info!("ctrl-D: end pty");
                            break;
                        } else if val[..].contains(&0x3_u8) {
                            // ctrl-C: end process
                            hdc::info!("ctrl-C: end process");
                            unsafe {
                                let tpgid = libc::tcgetpgrp(pty_process.pty.as_raw_fd());
                                if tpgid > 1 {
                                    libc::kill(tpgid,libc::SIGINT);
                                }
                            }
                            continue;
                        } else if val[..].contains(&0x11_u8) {
                            // ctrl-Q: dump process
                            hdc::info!("ctrl-Q: dump process");
                            let dump_message = task_manager::dump_running_task_info().await;
                            hdc::debug!("dump_message: {}", dump_message);
                            #[cfg(feature = "hdc_debug")]
                            let message = TaskMessage {
                                channel_id,
                                command: ret_command,
                                payload: dump_message.as_bytes().to_vec(),
                            };
                            #[cfg(feature = "hdc_debug")]
                            transfer::put(session_id, message).await;
                        }
                        if let Err(e) = pty_process.pty.write_all(&val).await {
                            hdc::warn!(
                                "session_id: {} channel_id: {}, pty write failed: {e:?}",
                                session_id, channel_id
                            );
                            break;
                        }
                    }
                    Err(e) => {
                        hdc::debug!("rx recv failed: {e:?}");
                    }
                }
            }
        }

        {
            let mut child_lock = pty_process.child.lock().await;
            let status = child_lock.try_wait();
            match status {
                Ok(Some(exit_status)) => {
                    hdc::debug!("interactive shell finish a process {:?}", exit_status);
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

        let kill_result = child_lock.kill().await;
        hdc::debug!("subprocess_task kill child(session_id {session_id}, channel_id {channel_id}), result:{:?}", kill_result);
        match child_lock.wait().await {
            Ok(exit_status) => {
                PtyMap::del(session_id, channel_id).await;
                hdc::debug!(
                    "subprocess_task waiting child exit success, status:{:?}.",
                    exit_status
                );
            }
            Err(e) => {
                let kill_result = child_lock.kill().await;
                hdc::debug!(
                    "subprocess_task child exit status {:?}, kill child, result:{:?}",
                    e,
                    kill_result
                );
            }
        }

        match child_lock.wait().await {
            Ok(exit_status) => {
                PtyMap::del(session_id, channel_id).await;
                hdc::debug!(
                    "subprocess_task waiting child exit success, status:{:?}.",
                    exit_status
                );
            }
            Err(e) => {
                hdc::debug!("subprocess_task waiting child exit fail, error:{:?}.", e);
            }
        }
    } else {
        let mut child_lock = pty_process.child.lock().await;
        hdc::debug!(
            "subprocess_task nohup_flag:{} wait before",
            pty_process.nohup_flag
        );
        let ret = child_lock.wait().await;
        PtyMap::del(session_id, channel_id).await;
        hdc::info!(
            "subprocess_task nohup_flag:{} wait after: {:#?}",
            pty_process.nohup_flag,
            ret
        );
    }

    let message = TaskMessage {
        channel_id,
        command: HdcCommand::KernelChannelClose,
        payload: vec![1],
    };
    transfer::put(session_id, message).await;
}

impl PtyTask {
    pub fn new(
        session_id: u32,
        channel_id: u32,
        option_cmd: Option<String>,
        ret_command: HdcCommand,
    ) -> Self {
        let (tx, rx) = ylong_runtime::sync::mpsc::bounded_channel::<Vec<u8>>(16);
        let cmd = option_cmd.clone();
        hdc::debug!("PtyTask new session_id {session_id}, channel_id {channel_id}");
        let handle = ylong_runtime::spawn(subprocess_task(
            option_cmd,
            session_id,
            channel_id,
            ret_command,
            rx,
        ));
        Self {
            handle,
            tx,
            session_id,
            channel_id,
            cmd,
        }
    }
}

impl Drop for PtyTask {
    fn drop(&mut self) {
        hdc::info!(
            "PtyTask Drop session_id {}, channel_id {}",
            self.session_id,
            self.channel_id
        );
    }
}

type Child_ = Arc<Mutex<ylongChild>>;
type PtyChildProcessMap_ = Arc<Mutex<HashMap<(u32, u32), Child_>>>;
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

    pub async fn get(session_id: u32, channel_id: u32) -> Option<Child_> {
        let pty_child_map = Self::get_instance();
        let map = pty_child_map.lock().await;
        if let Some(pty_child) = map.get(&(session_id, channel_id)) {
            return Some(pty_child.clone());
        }
        None
    }

    #[allow(unused)]
    pub async fn put(session_id: u32, channel_id: u32, pty_child: Child_) {
        let pty_child_map = Self::get_instance();
        let mut map = pty_child_map.lock().await;
        map.insert((session_id, channel_id), pty_child);
    }

    pub async fn del(session_id: u32, channel_id: u32) {
        let pty_child_map = Self::get_instance();
        let mut map = pty_child_map.lock().await;
        map.remove(&(session_id, channel_id));
    }
}

type PtyMap_ = Arc<Mutex<HashMap<(u32, u32), Arc<PtyTask>>>>;
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

    pub async fn get(session_id: u32, channel_id: u32) -> Option<Arc<PtyTask>> {
        let pty_map = Self::get_instance();
        let map = pty_map.lock().await;
        if let Some(pty_task) = map.get(&(session_id, channel_id)) {
            return Some(pty_task.clone());
        }
        None
    }

    pub async fn put(session_id: u32, channel_id: u32, pty_task: PtyTask) {
        let pty_map = Self::get_instance();
        let mut map = pty_map.lock().await;
        let arc_pty_task = Arc::new(pty_task);
        map.insert((session_id, channel_id), arc_pty_task);
    }

    pub async fn del(session_id: u32, channel_id: u32) {
        let pty_map = Self::get_instance();
        let mut map = pty_map.lock().await;
        map.remove(&(session_id, channel_id));

        PtyChildProcessMap::del(session_id, channel_id).await;
    }

    pub async fn stop_task(session_id: u32) {
        let pty_map = Self::get_instance();
        {
            let map = pty_map.lock().await;
            hdc::info!("hdc shell stop_task, session_id:{}, task_size: {}", session_id, map.len());
            for _iter in map.iter() {
                if _iter.0 .0 != session_id {
                    continue;
                }
                if let Some(pty_child) = PtyChildProcessMap::get(session_id, _iter.0 .1).await {
                    let mut child = pty_child.lock().await;
                    let kill_result = child.kill().await;
                    hdc::debug!(
                        "do map clear kill child, result:{:?}, session_id {}, channel_id {}",
                        kill_result,
                        session_id,
                        _iter.0 .1
                    );
                    match child.wait().await {
                        Ok(exit_status) => {
                            hdc::debug!(
                                "waiting child exit success, status:{:?}, session_id {}, channel_id {}",
                                exit_status,
                                session_id,
                                _iter.0.1
                            );
                        }
                        Err(e) => {
                            hdc::error!(
                                "waiting child exit fail, error:{:?}, session_id {}, channel_id {}",
                                e,
                                session_id,
                                _iter.0 .1
                            );
                        }
                    }
                    PtyChildProcessMap::del(session_id, _iter.0 .1).await;
                }
                hdc::debug!(
                    "Clear tty task, session_id: {}, channel_id:{}",
                    _iter.0 .0,
                    session_id
                );
            }
        }
    }

    pub async fn clear(session_id: u32) {
        hdc::info!("hdc shell stop_task, session_id:{}", session_id);
        let pty_map = Self::get_instance();
        let mut channel_list = Vec::new();
        {
            let map = pty_map.lock().await;
            for _iter in map.iter() {
                if _iter.0 .0 == session_id {
                    channel_list.push(_iter.0 .1);
                    if let Some(pty_child) = PtyChildProcessMap::get(session_id, _iter.0 .1).await {
                        let mut child = pty_child.lock().await;
                        let kill_result = child.kill().await;
                        hdc::debug!("do map clear kill child, result:{:?}", kill_result);
                        match child.wait().await {
                            Ok(exit_status) => {
                                hdc::debug!(
                                    "waiting child exit success, status:{:?}.",
                                    exit_status
                                );
                            }
                            Err(e) => {
                                hdc::info!("waiting child exit fail, error:{:?}.", e);
                            }
                        }
                        PtyChildProcessMap::del(session_id, _iter.0 .1).await;
                    }
                    hdc::debug!(
                        "Clear tty task, channel_id:{}, session_id: {}.",
                        _iter.0 .1,
                        session_id
                    );
                }
            }
        }
        {
            let mut map = pty_map.lock().await;
            for channel_id in channel_list {
                map.remove(&(session_id, channel_id));
            }
        }
    }

    pub async fn dump_task() -> String {
        let arc = Self::get_instance();
        let map = arc.lock().await;
        let mut result = String::new();
        for _iter in map.iter() {
            let command = match &_iter.1.cmd {
                Some(b) => b,
                _ => "",
            };
            result.push_str(&format!(
                "session_id:{},\tchannel_id:{},\tcommand:{}\n",
                _iter.1.session_id, _iter.1.channel_id, command
            ));
        }
        result
    }
}

pub async fn stop_task(session_id: u32) {
    PtyMap::stop_task(session_id).await;
}

pub async fn dump_task() -> String {
    PtyMap::dump_task().await
}
