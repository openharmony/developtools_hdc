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
use crate::auth;
use crate::parser;
use crate::task;

use hdc::config;
use hdc::config::HdcCommand;
use hdc::config::TaskMessage;
use hdc::transfer;
use hdc::transfer::host_usb;
use hdc::transfer::host_usb::HostUsbReader;
use hdc::transfer::host_usb::HostUsbWriter;
use hdc::utils;
use hdc::utils::hdc_log::*;
use std::process;
use std::str::FromStr;
use std::time::Duration;

use std::io::{self, Error, ErrorKind};

use ylong_runtime::net::{SplitReadHalf, SplitWriteHalf, TcpListener, TcpStream};

pub async fn run_server_mode(addr_str: String) -> io::Result<()> {
    start_usb_server().await;
    start_client_listen(addr_str).await
}

async fn start_usb_server() {
    let ptr = host_usb::init_host_usb() as u64;
    ylong_runtime::spawn(async move {
        loop {
            let buf = host_usb::get_ready_usb_devices(ptr);
            let device_list = String::from_utf8(host_usb::buf_to_vec(buf));
            match device_list {
                Ok(str) => {
                    if str.is_empty() {
                        std::thread::sleep(Duration::from_secs(1));
                        continue;
                    }
                    for sn in str.split(" ") {
                        if sn.is_empty() {
                            continue;
                        }
                        task::start_usb_device_loop(ptr, sn.to_string()).await;
                    }
                    std::thread::sleep(Duration::from_secs(1));
                }
                Err(_) => {
                    break;
                }
            }
        }
        host_usb::stop(ptr);
    });
}

async fn start_client_listen(addr_str: String) -> io::Result<()> {
    let saddr = addr_str;
    let listener = TcpListener::bind(saddr.clone()).await?;
    hdc::info!("server binds on {saddr}");

    loop {
        let (stream, addr) = listener.accept().await?;
        hdc::info!("accepted client {addr}");
        ylong_runtime::spawn(handle_client(stream));
    }
}

pub async fn get_process_pids() -> Vec<u32> {
    let mut pids: Vec<u32> = Vec::new();
    if cfg!(target_os = "windows") {
        let output = utils::execute_cmd("tasklist | findstr hdc".to_owned());
        let output_str = String::from_utf8_lossy(&output);
        let mut get_pid = false;
        for token in output_str.split_whitespace() {
            if get_pid {
                pids.push(u32::from_str(token).unwrap());
                get_pid = false;
            }
            if token.contains("exe") {
                get_pid = true;
            }
        }
    } else {
        let output =
            utils::execute_cmd("ps -ef | grep hdc | grep -v grep | awk '{{print $2}}'".to_owned());
        let output_str = String::from_utf8_lossy(&output);
        for pid in output_str.split_whitespace() {
            pids.push(u32::from_str(pid).unwrap());
        }
    }
    pids
}

// 跨平台命令
pub async fn check_allow_fork() -> bool {
    let pids = get_process_pids().await;
    for pid in pids {
        if pid != process::id() {
            println!("check_allow_fork return false");
            return false;
        }
    }
    true
}

// 跨平台命令
pub async fn server_fork(addr_str: String) {
    let current_exe = std::env::current_exe().unwrap().display().to_string();
    let result = process::Command::new(&current_exe)
        .args(["-b", "-m", "-s", addr_str.as_str()])
        .spawn();
    match result {
        Ok(_) => ylong_runtime::time::sleep(Duration::from_millis(1000)).await,
        Err(_) => hdc::info!("server fork failed"),
    }
}

pub async fn server_kill() {
    // TODO: check mac & win
    let pids = get_process_pids().await;
    hdc::info!("pid is {:?}", pids);
    for pid in pids {
        if pid != process::id() {
            if cfg!(target_os = "windows") {
                utils::execute_cmd(format!("taskkill /pid {} /f", pid));
            } else {
                utils::execute_cmd(format!("kill -9 {}", pid));
            }
        }
    }
}

#[allow(unused)]
#[derive(PartialEq)]
enum ChannelState {
    InteractiveShell,
    File,
    App,
    None,
}

async fn handle_client(stream: TcpStream) -> io::Result<()> {
    let (mut rd, wr) = stream.into_split();
    let (connect_key, channel_id) = handshake_with_client(&mut rd, wr).await?;
    let mut channel_state = ChannelState::None;

    loop {
        let recv_opt = transfer::tcp::recv_channel_message(&mut rd).await;
        if recv_opt.is_err() {
            let session_id = match task::ConnectMap::get_session_id(connect_key.clone()).await {
                Some(seid) => seid,
                None => return Ok(()),
            };
            let message = TaskMessage {
                channel_id,
                command: HdcCommand::KernelChannelClose,
                payload: vec![0],
            };
            transfer::put(session_id, message).await;
            return Ok(());
        }
        let recv = recv_opt.unwrap();
        hdc::debug!(
            "recv hex: {}",
            recv.iter()
                .map(|c| format!("{c:02x}"))
                .collect::<Vec<_>>()
                .join(" ")
        );

        let recv_str = String::from_utf8(recv.clone()).unwrap();
        hdc::debug!("recv str: {}", recv_str.clone());
        let mut parsed = parser::split_opt_and_cmd(
            String::from_utf8(recv)
                .unwrap()
                .split(' ')
                .map(|s| s.trim_end_matches('\0').to_string())
                .collect::<Vec<_>>(),
        );

        if channel_state == ChannelState::InteractiveShell {
            parsed.command = Some(HdcCommand::ShellData);
            parsed.parameters = vec![recv_str];
        }

        if parsed.command == Some(HdcCommand::UnityExecute) {
            channel_state = ChannelState::InteractiveShell;
            if parsed.parameters.len() == 1 {
                parsed.command = Some(HdcCommand::ShellInit);
            }
        }

        hdc::debug!("parsed cmd: {:#?}", parsed);

        if let Some(cmd) = parsed.command {
            if let Err(e) = task::channel_task_dispatch(task::TaskInfo {
                command: cmd,
                connect_key: connect_key.clone(),
                channel_id,
                params: parsed.parameters,
            })
            .await
            {
                hdc::error!("{e}");
            }
        } else {
            return Err(Error::new(ErrorKind::Other, "command not found"));
        }
    }
}

async fn handshake_with_client(
    rd: &mut SplitReadHalf,
    wr: SplitWriteHalf,
) -> io::Result<(String, u32)> {
    let channel_id = utils::get_pseudo_random_u32();
    transfer::TcpMap::start(channel_id, wr).await;

    let buf = [
        config::HANDSHAKE_MESSAGE.as_bytes(),
        vec![0_u8; config::BANNER_SIZE - config::HANDSHAKE_MESSAGE.len()].as_slice(),
        u32::to_le_bytes(channel_id).as_slice(),
        vec![0_u8; config::KEY_MAX_SIZE - std::mem::size_of::<u32>()].as_slice(),
    ]
    .concat();

    transfer::send_channel_data(channel_id, buf).await;
    let recv = transfer::tcp::recv_channel_message(rd).await.unwrap();
    let connect_key = unpack_channel_handshake(recv)?;
    Ok((connect_key, channel_id))
}

fn unpack_channel_handshake(recv: Vec<u8>) -> io::Result<String> {
    let msg = std::str::from_utf8(&recv[..config::HANDSHAKE_MESSAGE.len()]).unwrap();
    if msg != config::HANDSHAKE_MESSAGE {
        return Err(Error::new(ErrorKind::Other, "Recv server-hello failed"));
    }
    let key_buf = &recv[config::BANNER_SIZE..];
    let pos = match key_buf.iter().position(|c| *c == 0) {
        Some(p) => p,
        None => key_buf.len(),
    };
    if let Ok(connect_key) = String::from_utf8(key_buf[..pos].to_vec()) {
        Ok(connect_key)
    } else {
        Err(Error::new(ErrorKind::Other, "unpack connect key failed"))
    }
}
