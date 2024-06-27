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
//! daemon
#![allow(missing_docs)]

pub mod auth;
pub mod daemon_app;
pub mod daemon_unity;
pub mod mount;
pub mod shell;
pub mod task;
pub mod task_manager;
pub mod sys_para;

#[cfg(feature = "emulator")]
pub mod bridge;

use std::io::{self, ErrorKind};
use std::sync::Arc;
use std::ffi::c_int;
use crate::utils::{self, hdc_log::*};

use crate::common::jdwp::Jdwp;
use crate::config;
use crate::config::TaskMessage;
#[cfg(feature = "emulator")]
use crate::daemon_lib::bridge;
use crate::transfer;
#[cfg(not(feature = "emulator"))]
use crate::transfer::base::Reader;
#[cfg(not(feature = "emulator"))]
use crate::transfer::uart::UartReader;
#[cfg(not(feature = "emulator"))]
use crate::transfer::uart_wrapper;



use crate::daemon_lib::sys_para::*;


use std::ffi::CString;
#[cfg(not(feature = "emulator"))]
use ylong_runtime::net::{TcpListener, TcpStream};
#[cfg(not(feature = "emulator"))]
use ylong_runtime::sync::mpsc;

extern "C" {
    #[cfg(not(feature = "emulator"))]
    fn NeedDropRootPrivileges() -> c_int;
}

#[cfg(not(feature = "emulator"))]
pub fn need_drop_root_privileges() {
    crate::info!("need_drop_root_privileges");
    unsafe {
        NeedDropRootPrivileges();
    }
}

pub async fn handle_message(res: io::Result<TaskMessage>, session_id: u32) -> io::Result<()> {
    match res {
        Ok(msg) => {
            utils::spawn(async move {
                if let Err(e) = task::dispatch_task(msg, session_id).await {
                    crate::error!("dispatch tcp task failed: {}", e.to_string());
                }
            });
        }
        Err(e) => {
            crate::debug!("clear pty map: {}", session_id);
            if e.kind() == ErrorKind::Other {
                crate::warn!("unpack task failed: {}", e.to_string());
                return Err(e);
            }
        }
    };
    Ok(())
}

pub async fn jdwp_daemon_start(lock_value: Arc<Jdwp>) {
    lock_value.init().await;
}

#[cfg(feature = "emulator")]
pub async fn bridge_daemon_start() -> io::Result<()> {
    crate::info!("bridge_daemon_start start...");
    let ptr = bridge::init_bridge() as u64;
    crate::info!("bridge_daemon_start ptr:{}", ptr);
    let pipe_read_fd = bridge::start_listen(ptr);
    crate::info!("bridge_daemon_start pipe_read_fd:{}", pipe_read_fd);
    if pipe_read_fd < 0 {
        crate::error!("daemon bridge listen fail.");
        return Err(std::io::Error::new(
            ErrorKind::Other,
            "daemon bridge listen fail.",
        ));
    }
    loop {
        crate::info!("bridge_daemon_start loop...");
        let client_fd_for_hdc_server = bridge::accept_server_socket_fd(ptr, pipe_read_fd);
        if client_fd_for_hdc_server < 0 {
            crate::error!("bridge_daemon_start accept client fd for hdc server fail...");
            break;
        }
        let client_fd = bridge::init_client_fd(ptr, client_fd_for_hdc_server);
        if client_fd < 0 {
            crate::error!("bridge_daemon_start init client fd fail...");
            break;
        }
        utils::spawn(bridge_handle_client(
            ptr,
            client_fd,
            client_fd_for_hdc_server,
        ));
    }
    bridge::stop(ptr);
    Ok(())
}

#[cfg(feature = "emulator")]
pub async fn bridge_handle_client(ptr: u64, fd: i32, client_fd: i32) -> io::Result<()> {
    crate::info!("bridge_handle_client start...");
    let rd = bridge::BridgeReader { ptr, fd };
    let wr = bridge::BridgeWriter { ptr, fd };
    let recv_msg = bridge::unpack_task_message(&rd).await?;
    let (session_id, send_msg) = auth::handshake_init(recv_msg).await?;
    let channel_id = send_msg.channel_id;
    bridge::BridgeMap::start(session_id, wr).await;
    transfer::put(session_id, send_msg).await;

    if auth::AuthStatusMap::get(session_id).await == auth::AuthStatus::Ok {
        transfer::put(
            session_id,
            TaskMessage {
                channel_id,
                command: config::HdcCommand::KernelChannelClose,
                payload: vec![0],
            },
        )
        .await;
    }

    loop {
        let ret = handle_message(transfer::tcp::unpack_task_message(&rd).await, session_id).await;
        if ret.is_err() {
            unsafe {
                libc::close(fd);
                libc::close(client_fd);
            }
            break;
        }
    }
    Ok(())
}

#[cfg(not(feature = "emulator"))]
pub async fn tcp_handle_client(stream: TcpStream) -> io::Result<()> {
    let (mut rd, wr) = stream.into_split();
    let msg = transfer::tcp::unpack_task_message(&mut rd).await?;
    let session_id = auth::get_session_id_from_msg(&msg).await?;
    crate::info!(
        "tcp_handle_client session_id {session_id}, channel_id {}",
        msg.channel_id
    );
    transfer::TcpMap::start(session_id, wr).await;
    let ret = handle_message(Ok(msg), session_id).await;
    if ret.is_err() {
        transfer::TcpMap::end(session_id).await;
        return ret;
    }

    loop {
        let result = handle_message(
            transfer::tcp::unpack_task_message(&mut rd).await,
            session_id,
        )
        .await;
        if result.is_err() {
            crate::warn!("tcp free_session, session_id:{}, result:{:?}", session_id, result);
            task_manager::free_session(session_id).await;
            return result;
        }
    }
}

#[cfg(not(feature = "emulator"))]
pub async fn tcp_daemon_start(port: u16) -> io::Result<()> {
    crate::info!("tcp_daemon_start port = {:#?}", port);
    let saddr = format!("0.0.0.0:{}", port);
    let listener = TcpListener::bind(saddr.clone()).await?;
    let random_port = listener.local_addr()?.port();
    crate::info!(
        "daemon binds on saddr = {:#?}, port = {:#?}",
        saddr,
        random_port
    );
    if !set_dev_item(config::ENV_HOST_PORT, &random_port.to_string()) {
        crate::error!("set tcp port: {} failed.", port);
    }
    loop {
        let (stream, addr) = listener.accept().await?;
        crate::info!("accepted client {addr}");
        utils::spawn(async {
            if let Err(e) = tcp_handle_client(stream).await {
                crate::error!("tcp_handle_client {e:?}");
            }
        });
    }
}

#[cfg(not(feature = "emulator"))]
pub async fn uart_daemon_start() -> io::Result<()> {
    loop {
        let fd = transfer::uart::uart_init()?;
        if let Err(e) = uart_handle_client(fd).await {
            crate::error!("uart_handle_client failed, {:?}", e);
        }
        transfer::uart::uart_close(fd);
    }
}

#[cfg(not(feature = "emulator"))]
pub async fn uart_handshake(
    handshake_message: TaskMessage,
    fd: i32,
    rd: &UartReader,
    package_index: u32,
) -> io::Result<u32> {
    let (session_id, send_msg) = auth::handshake_init(handshake_message).await?;
    let channel_id = send_msg.channel_id;

    let wr = transfer::uart::UartWriter { fd };
    transfer::start_uart(session_id, wr).await;
    transfer::start_session(session_id).await;

    let Some(head) = rd.head.clone() else {
        return Err(std::io::Error::new(
            ErrorKind::Other,
            "rd head clone failed",
        ));
    };
    uart_wrapper::on_read_head(head).await;
    transfer::wrap_put(session_id, send_msg, package_index, 0).await;

    if auth::AuthStatusMap::get(session_id).await == auth::AuthStatus::Ok {
        transfer::put(
            session_id,
            TaskMessage {
                channel_id,
                command: config::HdcCommand::KernelChannelClose,
                payload: vec![0],
            },
        )
        .await;
    }
    Ok(session_id)
}

#[cfg(not(feature = "emulator"))]
pub async fn uart_handle_client(fd: i32) -> io::Result<()> {
    let mut rd = transfer::uart::UartReader { fd, head: None };
    let (packet_size, package_index) = rd.check_protocol_head()?;
    let (tx, mut rx) = mpsc::bounded_channel::<TaskMessage>(config::USB_QUEUE_LEN);
    utils::spawn(async move {
        let mut rd = transfer::uart::UartReader { fd, head: None };
        if let Err(e) =
            transfer::base::unpack_task_message_lock(&mut rd, packet_size, tx.clone()).await
        {
            crate::warn!("unpack task failed: {}, reopen fd...", e.to_string());
        }
    });
    let session_id;
    match rx.recv().await {
        Ok(handshake_message) => {
            let _ = rx.recv().await;
            crate::info!("uart handshake_message:{:?}", handshake_message);
            session_id = uart_handshake(handshake_message.clone(), fd, &rd, package_index).await?;
        }
        Err(e) => {
            crate::info!("uart handshake error, {e:?}");
            return Err(std::io::Error::new(
                ErrorKind::Other,
                format!("uart recv handshake error, {e:?}"),
            ));
        }
    }

    uart_wrapper::stop_other_session(session_id).await;
    let mut real_session_id = session_id;
    loop {
        let (packet_size, _package_index) = rd.check_protocol_head()?;
        let Some(head) = rd.head.clone() else {
            return Err(std::io::Error::new(ErrorKind::Other, "rd head clone file"));
        };
        let package_index = head.package_index;
        let session_id = head.session_id;
        uart_wrapper::on_read_head(head).await;
        if real_session_id != session_id {
            crate::info!("real_session_id:{real_session_id}, session_id:{session_id}");
            uart_wrapper::stop_other_session(session_id).await;
        }
        if packet_size == 0 {
            continue;
        }

        let (tx, mut rx) = mpsc::bounded_channel::<TaskMessage>(config::USB_QUEUE_LEN);
        utils::spawn(async move {
            let mut rd = transfer::uart::UartReader { fd, head: None };
            if let Err(e) =
                transfer::base::unpack_task_message_lock(&mut rd, packet_size, tx.clone()).await
            {
                crate::warn!("uart read uart taskmessage error:{:?}", e);
            }
        });

        loop {
            match rx.recv().await {
                Ok(message) => {
                    if message.command == config::HdcCommand::UartFinish {
                        break;
                    }

                    if message.command == config::HdcCommand::KernelHandshake {
                        real_session_id =
                            uart_handshake(message.clone(), fd, &rd, package_index).await?;
                        crate::info!("real_session_id:{real_session_id:?}");
                        continue;
                    }
                    let command = message.command;
                    utils::spawn(async move {
                        if let Err(e) = task::dispatch_task(message, real_session_id).await {
                            log::error!("dispatch task({:?}) fail: {:?}", command, e);
                        }
                    });
                }
                Err(e) => {
                    let error_msg = format!("uart recv error: {e:?}");
                    crate::info!("{error_msg}");
                    return Err(std::io::Error::new(ErrorKind::Other, error_msg));
                }
            }
        }
    }
}

#[cfg(not(feature = "emulator"))]
pub async fn usb_daemon_start() -> io::Result<()> {
    loop {
        let ret = transfer::usb::usb_init();
        match ret {
            Ok((config_fd, bulkin_fd, bulkout_fd)) => {
                let _ = usb_handle_client(config_fd, bulkin_fd, bulkout_fd).await;
                transfer::usb::usb_close(config_fd, bulkin_fd, bulkout_fd);
            }
            Err(e) => {
                crate::error!("usb init failure and restart hdcd error is {:?}", e);
                std::process::exit(0);
            }
        }
    }
}

#[cfg(not(feature = "emulator"))]
pub async fn usb_handle_client(_config_fd: i32, bulkin_fd: i32, bulkout_fd: i32) -> io::Result<()> {
    let _rd = transfer::usb::UsbReader { fd: bulkin_fd };
    let mut rx = transfer::usb_start_recv(bulkin_fd, 0);
    let mut cur_session_id = 0;
    loop {
        match rx.recv().await {
            Ok((msg, _index)) => {
                if msg.command == config::HdcCommand::KernelHandshake {
                    if let Ok(session_id_in_msg) = auth::get_session_id_from_msg(&msg).await {
                        if session_id_in_msg != cur_session_id {
                            crate::info!("new session(usb) id:{}", session_id_in_msg);
                            let wr = transfer::usb::UsbWriter { fd: bulkout_fd };
                            transfer::UsbMap::start(session_id_in_msg, wr).await;
                            task_manager::free_session(cur_session_id).await;
                            crate::debug!("free session(usb) sucessfully {:?}", cur_session_id);
                            cur_session_id = session_id_in_msg;
                        }
                    }
                }
                utils::spawn(async move {
                    if let Err(e) = task::dispatch_task(msg, cur_session_id).await {
                        crate::error!("dispatch task failed: {}", e.to_string());
                    }
                });
            }
            Err(e) => {
                crate::warn!("unpack task failed: {}", e.to_string());
                break;
            }
        }
    }
    task_manager::free_session(cur_session_id).await;
    Ok(())
}

#[cfg(not(feature = "emulator"))]
pub fn get_tcp_port() -> u16 {
    let (ret, host_port) = get_dev_item(config::ENV_HOST_PORT, "_");
    if !ret || host_port == "_" {
        crate::error!(
            "get host port failed, will use default port {}.",
            config::DAEMON_PORT
        );
        return config::DAEMON_PORT;
    }

    let str = host_port.trim();
    crate::info!("get_tcp_port from prop, value:{}", str);
    let mut end = str.len();
    for i in 0..str.len() {
        let c = str.as_bytes()[i];
        if !c.is_ascii_digit() {
            end = i;
            break;
        }
    }
    let str2 = str[0..end].to_string();
    let number = str2.parse::<u16>();
    if let Ok(num) = number {
        crate::info!("get host port:{} success", num);
        return num;
    }

    crate::error!(
        "convert host port failed, will use default port {}.",
        config::DAEMON_PORT
    );
    config::DAEMON_PORT
}
