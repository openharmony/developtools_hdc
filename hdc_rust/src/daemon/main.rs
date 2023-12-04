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

mod auth;
mod daemon_app;
mod daemon_unity;
use crate::jdwp::Jdwp;
mod mount;
mod shell;
mod task;
// mod sendmsg;

use std::io::{self, ErrorKind, Write};
use std::sync::Arc;
use std::time::SystemTime;

use crate::utils::hdc_log::*;

use hdc::common::jdwp;
use hdc::config;
use hdc::config::TaskMessage;
use hdc::transfer;
use hdc::utils;

use log::LevelFilter;
use std::ffi::CString;
use ylong_runtime::net::{TcpListener, TcpStream};

fn drop_root_privileges() -> bool {
    let user_name = "shell";
    let user_str = CString::new(user_name).unwrap();
    let group_names = vec!["shell", "log", "readproc"];
    let mut gids: Vec<libc::gid_t> = Vec::with_capacity(group_names.capacity());
    let user: *mut libc::passwd = unsafe { libc::getpwnam(user_str.as_ptr()) };
    if user.is_null() {
        hdc::error!("getpwuid {} fail", user_name);
        return false;
    }

    for group_name in &group_names {
        let g = <&str>::clone(group_name);
        let group: *mut libc::group = unsafe { libc::getgrnam(g.as_ptr()) };
        if group.is_null() {
            hdc::error!("calloc fail");
        } else {
            gids.push(unsafe { (*group).gr_gid });
        }
    }

    let mut ret = unsafe { libc::setuid((*user).pw_uid) };
    if ret != 0 {
        hdc::error!("setuid fail, {}", user_name);
        return false;
    }

    ret = unsafe { libc::setgid((*user).pw_gid) };
    if ret != 0 {
        hdc::error!("setgid fail, {}", user_name);
        return false;
    }

    ret = unsafe { libc::setgroups(group_names.capacity(), gids.as_ptr()) };
    if ret != 0 {
        hdc::error!("setgroups fail, {}", user_name);
        return false;
    }

    // selinux todo if (setcon)
    true
}

fn need_drop_root_privileges() -> bool {
    let (_, debug_mode) = utils::get_dev_item(config::ENV_DEBUGGABLE);
    let (_, root_mode) = utils::get_dev_item(config::ENV_ROOT_RUN_MODE);

    if debug_mode.starts_with('1') {
        if root_mode.starts_with('1') {
            let rc = unsafe { libc::setuid(0) };

            hdc::debug!("Root run rc: {:#?}", rc);
        } else if root_mode.starts_with('0') && unsafe { libc::getuid() == 0 } {
            {
                return drop_root_privileges();
            }
        }
    } else {
        return drop_root_privileges();
    }
    true
}

async fn handle_message(res: io::Result<TaskMessage>, session_id: u32) -> io::Result<()> {
    match res {
        Ok(msg) => {
        ylong_runtime::spawn(async move {
            if let Err(e) = task::dispatch_task(msg, session_id).await {
                hdc::error!("dispatch task failed: {}", e.to_string());
            }
        });
        }
        Err(e) => {
            if e.kind() == ErrorKind::Other {
                hdc::warn!("unpack task failed: {}", e.to_string());
                return Err(e);
            }
        }
    };
    Ok(())
}

async fn jdwp_daemon_start(lock_value: Arc<Jdwp>) {
    lock_value.init().await;
}

async fn tcp_handle_client(stream: TcpStream) -> io::Result<()> {
    let (mut rd, wr) = stream.into_split();
    let recv_msg = transfer::tcp::unpack_task_message(&mut rd).await?;

    let (session_id, send_msg) = auth::handshake_init(recv_msg).await?;
    let channel_id = send_msg.channel_id;

    transfer::TcpMap::start(session_id, wr).await;
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
        handle_message(
            transfer::tcp::unpack_task_message(&mut rd).await,
            session_id,
        )
        .await?;
    }
}

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

async fn uart_daemon_start() -> io::Result<()> {
    Ok(())
    // loop {
    //     let fd = transfer::uart::uart_init()?;
    //     uart_handle_client(fd).await?;
    // }
}

// async fn uart_handle_client(_fd: i32) -> io::Result<()> {
//     Ok(())
// let rd = transfer::uart::UartReader { fd };
// let wr = transfer::uart::UartWriter { fd };

// let recv_msg = transfer::base::unpack_task_message(&rd)?;
// let (session_id, send_msg) = auth::handshake_init(recv_msg).await?;
// let channel_id = send_msg.channel_id;

// transfer::UartMap::start(session_id, wr).await;
// transfer::put(session_id, send_msg).await;
// if auth::AuthStatusMap::get(session_id).await == auth::AuthStatus::Ok {
//     transfer::put(
//         session_id,
//         TaskMessage {
//             channel_id,
//             command: config::HdcCommand::KernelChannelClose,
//             payload: vec![0],
//         },
//     )
//     .await;
// }

// loop {
//     handle_message(transfer::base::unpack_task_message(&rd), session_id).await?;
// }
// }

async fn usb_daemon_start() -> io::Result<()> {
    loop {
        let (config_fd, bulkin_fd, bulkout_fd) = transfer::usb::usb_init()?;
        let _ = usb_handle_client(config_fd, bulkin_fd, bulkout_fd).await;
        transfer::usb::usb_close(config_fd, bulkin_fd, bulkout_fd);
    }
}

async fn usb_handle_client(_config_fd: i32, bulkin_fd: i32, bulkout_fd: i32) -> io::Result<()> {
    let _rd = transfer::usb::UsbReader { fd: bulkin_fd };
    let wr = transfer::usb::UsbWriter { fd: bulkout_fd };

    let mut rx = transfer::usb_start_recv(bulkin_fd, 0);
    let recv_msg = match rx.recv().await {
        Ok(msg) => msg,
        Err(_) => {
            return Err(utils::error_other("usb recv failed, reopen...".to_string()));
        }
    };

    // let recv_msg = transfer::base::unpack_task_message(&rd)?;
    let (session_id, send_msg) = auth::handshake_init(recv_msg).await?;
    let channel_id = send_msg.channel_id;

    transfer::UsbMap::start(session_id, wr).await;
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
        match rx.recv().await {
            Ok(msg) => {
                ylong_runtime::spawn(async move {
                    if let Err(e) = task::dispatch_task(msg, session_id).await {
                        hdc::error!("dispatch task failed: {}", e.to_string());
                    }
                });
            }
            Err(e) => {
                hdc::warn!("unpack task failed: {}", e.to_string());
                break;
            }
        }
    }
    Ok(())
}

fn logger_init(log_level: LevelFilter) {
    env_logger::Builder::new()
        .format(|buf, record| {
            let ts = humantime::format_rfc3339_millis(SystemTime::now()).to_string();
            let level = &record.level().to_string()[..1];
            let file = record.file().unwrap();
            writeln!(
                buf,
                "{} {} {} {}:{} - {}",
                &ts[..10],
                &ts[11..23],
                level,
                file.split('/').last().unwrap(),
                record.line().unwrap(),
                record.args()
            )
        })
        .filter(None, log_level)
        .init();
}

fn get_logger_lv() -> LevelFilter {
    let lv = match std::env::var_os("HDCD_LOGLV") {
        None => 0_usize,
        // no need to prevent panic here
        Some(lv) => lv.to_str().unwrap().parse::<usize>().unwrap(),
    };
    config::LOG_LEVEL_ORDER[lv]
}

fn get_tcp_port() -> u16 {
    let shell_command = format!("{} {}", config::SHELL_PARAM_GET, config::ENV_HOST_PORT,);
    let result = utils::execute_cmd(shell_command);
    let str_result = String::from_utf8(result);
    if let Ok(str) = str_result {
        println!("get_tcp_port from prop,value:{}", str);
        let mut end = 0;
        for i in 0..str.len() - 1 {
            let c = str.as_bytes()[i];
            if !c.is_ascii_digit() {
                end = i;
                break;
            }
        }
        let str2 = str[0..end].to_string();
        let number = str2.parse::<u16>();
        if let Ok(num) = number {
            println!("num:{}", num);
            return num;
        } else {
            println!("num error");
        }
    }
    config::DAEMON_PORT
}

fn main() {
    logger_init(get_logger_lv());

    let _ = ylong_runtime::builder::RuntimeBuilder::new_multi_thread()
        .worker_stack_size(16 * 1024 * 1024)
        .worker_num(256)
        .keep_alive_time(std::time::Duration::from_secs(10))
        .build_global();

    ylong_runtime::block_on(async {
        need_drop_root_privileges();
        let tcp_task = ylong_runtime::spawn(async {
            if let Err(e) = tcp_daemon_start(get_tcp_port()).await {
                println!("[Fail]tcp daemon failed: {}", e);
            }
        });
        let usb_task = ylong_runtime::spawn(async {
            if let Err(e) = usb_daemon_start().await {
                println!("[Fail]usb daemon failed: {}", e);
            }
        });
        let uart_task = ylong_runtime::spawn(async {
            if let Err(e) = uart_daemon_start().await {
                println!("[Fail]uart daemon failed: {}", e);
            }
        });
        let lock_value = Jdwp::get_instance();
        let jdwp_server_task = ylong_runtime::spawn(async {
            jdwp_daemon_start(lock_value).await;
        });
        let _ = tcp_task.await;
        let _ = usb_task.await;
        let _ = uart_task.await;
        let _ = jdwp_server_task.await;
    });
}
