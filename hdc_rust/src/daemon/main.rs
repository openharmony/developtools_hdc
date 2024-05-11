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

extern crate panic_handler;
mod auth;
mod daemon_app;
mod daemon_unity;
use crate::jdwp::Jdwp;
mod mount;
mod shell;
mod task;
mod task_manager;
// mod sendmsg;
mod sys_para;

use std::io::{self, ErrorKind, Write};
use std::sync::Arc;
use std::time::SystemTime;

use crate::utils::hdc_log::*;

use crate::shell::PtyMap;
use hdc::common::jdwp;
use hdc::config;
use hdc::config::TaskMessage;
#[cfg(feature = "emulator")]
use hdc::daemon_lib::bridge;
use hdc::transfer;
#[cfg(not(feature = "emulator"))]
use hdc::transfer::base::Reader;
#[cfg(not(feature = "emulator"))]
use hdc::transfer::uart::UartReader;
#[cfg(not(feature = "emulator"))]
use hdc::transfer::uart_wrapper;
use hdc::utils;

use crate::auth::clear_auth_pub_key_file;
use crate::sys_para::*;
use log::LevelFilter;
#[cfg(not(feature = "emulator"))]
use std::ffi::c_int;
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
fn need_drop_root_privileges() {
    hdc::info!("need_drop_root_privileges");
    unsafe {
        NeedDropRootPrivileges();
    }
}

async fn handle_message(res: io::Result<TaskMessage>, session_id: u32) -> io::Result<()> {
    match res {
        Ok(msg) => {
            if let Err(e) = task::dispatch_task(msg, session_id).await {
                hdc::error!("dispatch task failed: {}", e.to_string());
            }
        }
        Err(e) => {
            hdc::debug!("clear pty map: {}", session_id);
            if e.kind() == ErrorKind::Other {
                hdc::warn!("unpack task failed: {}", e.to_string());
                PtyMap::clear(session_id).await;
                return Err(e);
            }
        }
    };
    Ok(())
}

async fn jdwp_daemon_start(lock_value: Arc<Jdwp>) {
    lock_value.init().await;
}

#[cfg(feature = "emulator")]
async fn bridge_daemon_start() -> io::Result<()> {
    hdc::info!("bridge_daemon_start start...");
    let ptr = bridge::init_bridge() as u64;
    hdc::info!("bridge_daemon_start ptr:{}", ptr);
    let pipe_read_fd = bridge::start_listen(ptr);
    hdc::info!("bridge_daemon_start pipe_read_fd:{}", pipe_read_fd);
    if pipe_read_fd < 0 {
        hdc::error!("daemon bridge listen fail.");
        return Err(std::io::Error::new(
            ErrorKind::Other,
            "daemon bridge listen fail.",
        ));
    }
    loop {
        hdc::info!("bridge_daemon_start loop...");
        let client_fd_for_hdc_server = bridge::accept_server_socket_fd(ptr, pipe_read_fd);
        if client_fd_for_hdc_server < 0 {
            hdc::error!("bridge_daemon_start accept client fd for hdc server fail...");
            break;
        }
        let client_fd = bridge::init_client_fd(ptr, client_fd_for_hdc_server);
        if client_fd < 0 {
            hdc::error!("bridge_daemon_start init client fd fail...");
            break;
        }
        ylong_runtime::spawn(bridge_handle_client(
            ptr,
            client_fd,
            client_fd_for_hdc_server,
        ));
    }
    bridge::stop(ptr);
    Ok(())
}

#[cfg(feature = "emulator")]
async fn bridge_handle_client(ptr: u64, fd: i32, client_fd: i32) -> io::Result<()> {
    hdc::info!("bridge_handle_client start...");
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
async fn tcp_handle_client(stream: TcpStream) -> io::Result<()> {
    let (mut rd, wr) = stream.into_split();
    let msg = transfer::tcp::unpack_task_message(&mut rd).await?;
    let session_id = auth::get_session_id_from_msg(&msg).await?;
    transfer::TcpMap::start(session_id, wr).await;
    handle_message(Ok(msg), session_id).await?;

    loop {
        handle_message(
            transfer::tcp::unpack_task_message(&mut rd).await,
            session_id,
        )
        .await?;
    }
}

#[cfg(not(feature = "emulator"))]
async fn tcp_daemon_start(port: u16) -> io::Result<()> {
    let saddr = format!("0.0.0.0:{}", port);
    let listener = TcpListener::bind(saddr.clone()).await?;
    hdc::info!("daemon binds on {saddr}");
    loop {
        let (stream, addr) = listener.accept().await?;
        hdc::info!("accepted client {addr}");
        ylong_runtime::spawn(tcp_handle_client(stream));
    }
}

#[cfg(not(feature = "emulator"))]
async fn uart_daemon_start() -> io::Result<()> {
    loop {
        let fd = transfer::uart::uart_init()?;
        let _ret = uart_handle_client(fd).await;
        transfer::uart::uart_close(fd);
    }
}

#[cfg(not(feature = "emulator"))]
async fn uart_handshake(
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

    let head = rd.head.clone().unwrap();
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
async fn uart_handle_client(fd: i32) -> io::Result<()> {
    let mut rd = transfer::uart::UartReader { fd, head: None };
    let (packet_size, package_index) = rd.check_protocol_head()?;
    let (tx, mut rx) = mpsc::bounded_channel::<TaskMessage>(config::USB_QUEUE_LEN);
    ylong_runtime::spawn(async move {
        let mut rd = transfer::uart::UartReader { fd, head: None };
        if let Err(e) =
            transfer::base::unpack_task_message_lock(&mut rd, packet_size, tx.clone()).await
        {
            hdc::warn!("unpack task failed: {}, reopen fd...", e.to_string());
            hdc::info!("handshake error:{:#?}", e);
        }
    });
    let session_id;
    match rx.recv().await {
        Ok(handshake_message) => {
            let _ = rx.recv().await;
            hdc::info!("uart handshake_message:{:#?}", handshake_message);
            session_id = uart_handshake(handshake_message.clone(), fd, &rd, package_index).await?;
        }
        Err(_e) => {
            hdc::info!("uart handshake error");
            return Err(std::io::Error::new(
                ErrorKind::Other,
                "uart recv handshake error",
            ));
        }
    }

    uart_wrapper::stop_other_session(session_id).await;
    let mut real_session_id = session_id;
    loop {
        let (packet_size, _package_index) = rd.check_protocol_head()?;
        let head = rd.head.clone().unwrap();
        let package_index = head.package_index;
        let session_id = head.session_id;
        uart_wrapper::on_read_head(head).await;
        if real_session_id != session_id {
            uart_wrapper::stop_other_session(session_id).await;
        }
        if packet_size == 0 {
            continue;
        }

        let (tx, mut rx) = mpsc::bounded_channel::<TaskMessage>(config::USB_QUEUE_LEN);
        ylong_runtime::spawn(async move {
            let mut rd = transfer::uart::UartReader { fd, head: None };
            if let Err(e) =
                transfer::base::unpack_task_message_lock(&mut rd, packet_size, tx.clone()).await
            {
                hdc::warn!("unpack task failed: {}, reopen fd...", e.to_string());
                hdc::info!("uart read uart taskmessage error:{:#?}", e);
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
                        continue;
                    }
                    let command = message.command;
                    ylong_runtime::spawn(async move {
                        if let Err(e) = task::dispatch_task(message, real_session_id).await {
                            log::error!("dispatch task failed: {}", e.to_string());
                            hdc::info!("dispatch task({:#?}) fail: {:#?}", command, e);
                        }
                    });
                }
                Err(_e) => {
                    hdc::info!("uart recv error: {:#?}", _e);
                    return Err(std::io::Error::new(ErrorKind::Other, "RecvError"));
                }
            }
        }
    }
}

#[cfg(not(feature = "emulator"))]
async fn usb_daemon_start() -> io::Result<()> {
    loop {
        let ret = transfer::usb::usb_init();
        match ret {
            Ok((config_fd, bulkin_fd, bulkout_fd)) => {
                let _ = usb_handle_client(config_fd, bulkin_fd, bulkout_fd).await;
                transfer::usb::usb_close(config_fd, bulkin_fd, bulkout_fd);
            }
            Err(e) => {
                hdc::error!("usb inut failure and restart hdcd error is {:#?}", e);
                std::process::exit(0);
            }
        }
    }
}

#[cfg(not(feature = "emulator"))]
async fn usb_handle_client(_config_fd: i32, bulkin_fd: i32, bulkout_fd: i32) -> io::Result<()> {
    let _rd = transfer::usb::UsbReader { fd: bulkin_fd };
    let mut rx = transfer::usb_start_recv(bulkin_fd, 0);
    let mut cur_session_id = 0;
    loop {
        match rx.recv().await {
            Ok((msg, _index)) => {
                if msg.command == config::HdcCommand::KernelHandshake {
                    if let Ok(session_id_in_msg) = auth::get_session_id_from_msg(&msg).await {
                        if session_id_in_msg != cur_session_id {
                            hdc::info!("new session id:{}", session_id_in_msg);
                            let wr = transfer::usb::UsbWriter { fd: bulkout_fd };
                            transfer::UsbMap::start(session_id_in_msg, wr).await;
                            task_manager::free_session(
                                config::ConnectType::Usb("some_mount_point".to_string()),
                                cur_session_id,
                            )
                            .await;
                            PtyMap::clear(cur_session_id).await;
                            cur_session_id = session_id_in_msg;
                        }
                    }
                }
                ylong_runtime::spawn(async move {
                    if let Err(e) = task::dispatch_task(msg, cur_session_id).await {
                        hdc::error!("dispatch task failed: {}", e.to_string());
                    }
                });
            }
            Err(e) => {
                hdc::warn!("unpack task failed: {}", e.to_string());
                PtyMap::clear(cur_session_id).await;
                break;
            }
        }
    }
    task_manager::free_session(
        config::ConnectType::Usb("some_mount_point".to_string()),
        cur_session_id,
    )
    .await;
    Ok(())
}

fn logger_init(log_level: LevelFilter) {
    env_logger::Builder::new()
        .format(|buf, record| {
            let ts = humantime::format_rfc3339_millis(SystemTime::now()).to_string();
            let level = &record.level().to_string()[..1];
            let file = record.file().unwrap_or("unknown");
            writeln!(
                buf,
                "{} {} {} {}:{} - {}",
                &ts[..10],
                &ts[11..23],
                level,
                file.split('/').last().unwrap_or("unknown"),
                record.line().unwrap_or(0),
                record.args()
            )
        })
        .filter(None, log_level)
        .init();
}

fn get_logger_lv() -> LevelFilter {
    let lv = std::env::var_os("HDCD_LOGLV")
        .unwrap_or_default()
        .to_str()
        .unwrap_or_default()
        .parse::<usize>()
        .unwrap_or(0_usize);
    config::LOG_LEVEL_ORDER[lv]
}

#[cfg(not(feature = "emulator"))]
fn get_tcp_port() -> u16 {
    let (ret, host_port) = get_dev_item(config::ENV_HOST_PORT, "_");
    if !ret || host_port == "_" {
        hdc::info!(
            "get host port failed, will use default port {}.",
            config::DAEMON_PORT
        );
        return config::DAEMON_PORT;
    }

    let str = host_port.trim();
    hdc::info!("get_tcp_port from prop, value:{}", str);
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
        hdc::info!("get host port:{} success", num);
        return num;
    }

    hdc::info!(
        "convert host port failed, will use default port {}.",
        config::DAEMON_PORT
    );
    config::DAEMON_PORT
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    panic_handler::init();
    if args.len() == 2 && args[1] == "-v" {
        hdc::info!("{}", config::get_version());
        return;
    }
    logger_init(get_logger_lv());

    let _ = ylong_runtime::builder::RuntimeBuilder::new_multi_thread()
        .worker_stack_size(16 * 1024 * 1024)
        .worker_num(256)
        .keep_alive_time(std::time::Duration::from_secs(10))
        .build_global();

    #[cfg(not(feature = "emulator"))]
    need_drop_root_privileges();
    clear_auth_pub_key_file();

    ylong_runtime::block_on(async {
        #[cfg(not(feature = "emulator"))]
        let tcp_task = ylong_runtime::spawn(async {
            if let Err(e) = tcp_daemon_start(get_tcp_port()).await {
                hdc::info!("[Fail]tcp daemon failed: {}", e);
            }
        });
        #[cfg(not(feature = "emulator"))]
        let usb_task = ylong_runtime::spawn(async {
            if let Err(e) = usb_daemon_start().await {
                hdc::info!("[Fail]usb daemon failed: {}", e);
            }
        });
        #[cfg(not(feature = "emulator"))]
        let uart_task = ylong_runtime::spawn(async {
            if let Err(e) = uart_daemon_start().await {
                hdc::info!("[Fail]uart daemon failed: {}", e);
            }
        });
        #[cfg(feature = "emulator")]
        hdc::info!("daemon main emulator, start bridge daemon.");
        #[cfg(feature = "emulator")]
        let bridge_task = ylong_runtime::spawn(async {
            if let Err(e) = bridge_daemon_start().await {
                println!("[Fail]bridge daemon failed: {}", e);
            }
        });
        let lock_value = Jdwp::get_instance();
        let jdwp_server_task = ylong_runtime::spawn(async {
            jdwp_daemon_start(lock_value).await;
        });
        #[cfg(not(feature = "emulator"))]
        let _ = tcp_task.await;
        #[cfg(not(feature = "emulator"))]
        let _ = usb_task.await;
        #[cfg(not(feature = "emulator"))]
        let _ = uart_task.await;
        #[cfg(feature = "emulator")]
        let _ = bridge_task.await;
        let _ = jdwp_server_task.await;
    });
}
