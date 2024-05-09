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
use crate::config::*;
use crate::host_app;
use crate::host_app::HostAppTaskMap;
/// ActionType 未定义，临时屏蔽
/// use crate::host_app::HostAppTask;
/// use hdc::common::hdcfile::HdcFile;
use hdc::common::hdcfile::{self, FileTaskMap, HdcFile};
use hdc::config::{HdcCommand, ConnectType};
use hdc::transfer;
use hdc::host_transfer::host_usb;
use hdc::utils;

use std::collections::HashMap;
use std::io::{self, Error, ErrorKind};
use std::sync::Arc;

#[cfg(feature = "host")]
extern crate ylong_runtime_static as ylong_runtime;
use ylong_runtime::net::SplitReadHalf;
use ylong_runtime::net::TcpStream;
use ylong_runtime::sync::{Mutex, RwLock};

use crate::host_app::HostAppTask;

#[derive(Debug, Clone)]
pub struct TaskInfo {
    pub command: HdcCommand,
    pub connect_key: String,
    pub channel_id: u32,
    pub params: Vec<String>,
}

pub async fn channel_task_dispatch(task_info: TaskInfo) -> io::Result<()> {
    hdc::debug!(
        "in channel_task_dispatch, task_info={:#?}",
        task_info.clone()
    );

    match task_info.command {
        HdcCommand::UnityRunmode | HdcCommand::UnityRootrun => {
            hdc::trace!("dispatch to runmode task");
            channel_unity_task(task_info).await?
        }
        HdcCommand::UnityExecute | HdcCommand::ShellInit | HdcCommand::ShellData => {
            hdc::trace!("dispatch to shell task");
            channel_shell_task(task_info).await?
        }
        HdcCommand::KernelTargetConnect => {
            hdc::trace!("dispatch to tconn task");
            channel_connect_task(task_info).await?;
        }
        HdcCommand::KernelTargetList => {
            hdc::trace!("dispatch to list task");
            channel_list_targets_task(task_info).await?;
        }
        HdcCommand::KernelWaitFor => {
            hdc::trace!("dispatch to wait");
            channel_wait_for_any(task_info).await?;
        }
        HdcCommand::KernelChannelClose => {
            hdc::trace!("dispatch to close task");
            transfer::TcpMap::end(task_info.channel_id).await;
        }
        HdcCommand::FileInit
        | HdcCommand::FileBegin
        | HdcCommand::FileData
        | HdcCommand::FileCheck
        | HdcCommand::FileFinish
        | HdcCommand::AppInit
        | HdcCommand::AppBegin
        | HdcCommand::AppData
        | HdcCommand::AppFinish
        | HdcCommand::AppUninstall => {
            channel_file_task(task_info).await?;
        }
        HdcCommand::FileRecvInit => {
            send_to_daemon(task_info, HdcCommand::FileInit).await?;
        }
        HdcCommand::UnityHilog => {
            channel_hilog_task(task_info).await?;
        }
        HdcCommand::UnityBugreportInit => {
            channel_bug_report_task(task_info).await?;
        }

        HdcCommand::JdwpList | HdcCommand::JdwpTrack => {
            channel_jdwp_task(task_info).await?;
        }
        HdcCommand::KernelCheckServer => {
            check_server_task(task_info).await?;
        }
        _ => {
            hdc::info!("get unknown command {:#?}", task_info.command);
            return Err(Error::new(ErrorKind::Other, "command not found"));
        }
    }
    Ok(())
}

async fn channel_jdwp_task(task_info: TaskInfo) -> io::Result<()> {
    let session_id =
        get_valid_session_id(task_info.connect_key.clone(), task_info.channel_id).await?;
    let payload = task_info.params.join(" ").into_bytes();
    transfer::put(
        session_id,
        TaskMessage {
            channel_id: task_info.channel_id,
            command: task_info.command,
            payload,
        },
    )
    .await;
    Ok(())
}

async fn channel_hilog_task(task_info: TaskInfo) -> io::Result<()> {
    let session_id =
        get_valid_session_id(task_info.connect_key.clone(), task_info.channel_id).await?;
    let payload = if task_info.params.len() > 1 && task_info.params[1] == "-h" {
        vec![104]
    } else {
        vec![0]
    };
    transfer::put(
        session_id,
        TaskMessage {
            channel_id: task_info.channel_id,
            command: HdcCommand::UnityHilog,
            payload,
        },
    )
    .await;
    Ok(())
}

async fn channel_bug_report_task(task_info: TaskInfo) -> io::Result<()> {
    let session_id =
        get_valid_session_id(task_info.connect_key.clone(), task_info.channel_id).await?;
    transfer::put(
        session_id,
        TaskMessage {
            channel_id: task_info.channel_id,
            command: HdcCommand::UnityBugreportInit,
            payload: vec![],
        },
    )
    .await;
    Ok(())
}

async fn channel_file_task(task_info: TaskInfo) -> io::Result<()> {
    let session_id =
        get_valid_session_id(task_info.connect_key.clone(), task_info.channel_id).await?;
    let payload = task_info.params.join(" ").into_bytes();
    match task_info.command {
        HdcCommand::AppInit | HdcCommand::AppUninstall => {
            if !HostAppTaskMap::exist(session_id, task_info.channel_id)
                .await
                .unwrap()
            {
                HostAppTaskMap::put(
                    session_id,
                    task_info.channel_id,
                    HostAppTask::new(session_id, task_info.channel_id),
                )
                .await;
            };
            let _ = host_app::command_dispatch(
                session_id,
                task_info.channel_id,
                task_info.command,
                &payload,
                payload.len() as u16,
            )
            .await;
        }

        HdcCommand::FileCheck | HdcCommand::FileInit => {
            if !FileTaskMap::exsit(session_id, task_info.channel_id).await {
                let mut task = HdcFile::new(session_id, task_info.channel_id);
                task.transfer.server_or_daemon = true;
                FileTaskMap::put(session_id, task_info.channel_id, task).await;
            }
            hdcfile::command_dispatch(
                session_id,
                task_info.channel_id,
                task_info.command,
                &payload,
                payload.len() as u16)
                .await;
            return Ok(());
        }
        HdcCommand::FileBegin | HdcCommand::FileData | HdcCommand::FileFinish => {
            hdcfile::command_dispatch(
                session_id,
                task_info.channel_id,
                task_info.command,
                &payload,
                payload.len() as u16)
                .await;
                return Ok(());
        }
        _ => {
            println!("other tasks, payload is {:#?}", payload);
        }
    }
    Ok(())
}

async fn send_to_daemon(task_info: TaskInfo, _cmd: HdcCommand) -> io::Result<()> {
    let session_id =
        get_valid_session_id(task_info.connect_key.clone(), task_info.channel_id).await?;
    transfer::put(
        session_id,
        TaskMessage {
            channel_id: task_info.channel_id,
            command: _cmd,
            payload: task_info.params[2..].join(" ").into_bytes(),
        },
    )
    .await;
    Ok(())
}

async fn channel_unity_task(task_info: TaskInfo) -> io::Result<()> {
    let session_id = match ConnectMap::get_session_id(task_info.connect_key.clone()).await {
        Some(seid) => seid,
        None => return Err(Error::new(ErrorKind::Other, "session not found")),
    };
    let cmd = task_info.params[1..]
        .iter()
        .map(|s| s.trim_end_matches('\0'))
        .collect::<Vec<_>>()
        .join(" ")
        .into_bytes();
    transfer::put(
        session_id,
        TaskMessage {
            channel_id: task_info.channel_id,
            command: task_info.command,
            payload: cmd,
        },
    )
    .await;
    Ok(())
}

async fn channel_shell_task(task_info: TaskInfo) -> io::Result<()> {
    let session_id =
        get_valid_session_id(task_info.connect_key.clone(), task_info.channel_id).await?;
    match task_info.command {
        HdcCommand::UnityExecute => {
            let cmd = task_info.params[1..]
                .iter()
                .map(|s| s.trim_end_matches('\0'))
                .collect::<Vec<_>>()
                .join(" ")
                .into_bytes();
            transfer::put(
                session_id,
                TaskMessage {
                    channel_id: task_info.channel_id,
                    command: task_info.command,
                    payload: cmd,
                },
            )
            .await;
        }
        HdcCommand::ShellInit => {
            transfer::put(
                session_id,
                TaskMessage {
                    channel_id: task_info.channel_id,
                    command: task_info.command,
                    payload: vec![0],
                },
            )
            .await;
        }
        HdcCommand::ShellData => {
            let payload = task_info.params.join("").into_bytes();
            transfer::put(
                session_id,
                TaskMessage {
                    channel_id: task_info.channel_id,
                    command: task_info.command,
                    payload,
                },
            )
            .await;
        }
        _ => {}
    }

    Ok(())
}

async fn channel_connect_task(task_info: TaskInfo) -> io::Result<()> {
    let connect_key = task_info.params[1].trim_end_matches('\0').to_string();
    if ConnectMap::get(connect_key.clone()).await.is_some() {
        let ret = transfer::send_channel_msg(
            task_info.channel_id,
            transfer::EchoLevel::INFO,
            "Target is connected, repeat operation".to_string(),
        )
        .await;
        transfer::TcpMap::end(task_info.channel_id).await;
        return ret;
    }
    start_tcp_daemon_session(connect_key, &task_info).await
}

pub async fn usb_handle_deamon(ptr: u64, session_id: u32, connect_key: String) -> io::Result<()> {
    let mut rx = host_usb::start_recv(ptr, connect_key.clone(), session_id);
    loop {
        match rx.recv().await {
            Ok((task_message, _index)) => {
                hdc::debug!(
                    "in usb_handle_deamon, recv cmd: {:#?}, payload len: {}",
                    task_message.command,
                    task_message.payload.len(),
                );
                if let Err(e) = session_task_dispatch(task_message, session_id).await {
                    hdc::error!("dispatch task failed: {}", e.to_string());
                }
            }
            Err(e) => {
                hdc::warn!("unpack task failed: {}", e.to_string());
                ConnectMap::remove(connect_key.clone()).await;
                host_usb::on_device_connected(ptr, connect_key.clone(), false);
                return Err(Error::new(ErrorKind::Other, "recv error"));
            }
        };
    }
}

pub async fn start_usb_device_loop(ptr: u64, connect_key: String) {
    let session_id = utils::get_pseudo_random_u32();
    let channel_id = utils::get_pseudo_random_u32();
    let wr = host_usb::HostUsbWriter {
        connect_key: connect_key.clone(),
        ptr,
    };
    host_usb::HostUsbMap::start(session_id, wr).await;
    match auth::usb_handshake_with_daemon(ptr, connect_key.clone(), session_id, channel_id).await {
        Ok((dev_name, version)) => {
            host_usb::on_device_connected(ptr, connect_key.clone(), true);
            ConnectMap::put(
                connect_key.clone(),
                DaemonInfo {
                    session_id,
                    conn_type: ConnectType::HostUsb(connect_key.clone()),
                    conn_status: ConnectStatus::Connected,
                    dev_name,
                    version,
                },
            )
            .await;
        }
        Err(e) => {
            let _ =
                transfer::send_channel_msg(channel_id, transfer::EchoLevel::FAIL, e.to_string())
                    .await;
        }
    };
    ylong_runtime::spawn(usb_handle_deamon(ptr, session_id, connect_key));
}

async fn start_tcp_daemon_session(connect_key: String, task_info: &TaskInfo) -> io::Result<()> {
    match TcpStream::connect(connect_key.clone()).await {
        Err(_) => {
            let ret = transfer::send_channel_msg(
                task_info.channel_id,
                transfer::EchoLevel::FAIL,
                "Connect to daemon failed".to_string(),
            )
            .await;
            transfer::TcpMap::end(task_info.channel_id).await;
            ret
        }
        Ok(stream) => {
            let session_id = utils::get_pseudo_random_u32();
            let (mut rd, wr) = stream.into_split();
            transfer::TcpMap::start(session_id, wr).await;

            match auth::handshake_with_daemon(
                connect_key.clone(),
                session_id,
                task_info.channel_id,
                &mut rd,
            )
            .await
            {
                Ok((dev_name, version)) => {
                    ConnectMap::put(
                        connect_key.clone(),
                        DaemonInfo {
                            session_id,
                            conn_type: ConnectType::Tcp,
                            conn_status: ConnectStatus::Connected,
                            dev_name,
                            version,
                        },
                    )
                    .await;
                }
                Err(e) => {
                    let _ = transfer::send_channel_msg(
                        task_info.channel_id,
                        transfer::EchoLevel::FAIL,
                        e.to_string(),
                    )
                    .await;
                    transfer::TcpMap::end(task_info.channel_id).await;
                    return Ok(());
                }
            };

            ylong_runtime::spawn(tcp_handle_deamon(rd, session_id, connect_key));
            transfer::send_channel_msg(
                task_info.channel_id,
                transfer::EchoLevel::INFO,
                "Connect OK".to_string(),
            ).await?;
            transfer::TcpMap::end(task_info.channel_id).await;
            Ok(())
        }
    }
}

async fn channel_list_targets_task(task_info: TaskInfo) -> io::Result<()> {
    let is_full = task_info.params.contains(&"-v".to_string());
    let target_list = ConnectMap::get_list(is_full).await;
    let msg = if target_list.is_empty() {
        "[Empty]".to_string()
    } else {
        target_list.join("\n")
    };
    transfer::send_channel_msg(task_info.channel_id, transfer::EchoLevel::RAW, msg).await?;
    transfer::TcpMap::end(task_info.channel_id).await;
    Ok(())
}

// check if any daemon connected and send the message to client for wait
async fn channel_wait_for_any(task_info: TaskInfo) -> io::Result<()> {
    let target_list = ConnectMap::get_list(false).await;
    if target_list.is_empty() {
        hdc::info!("No any connected target");
        let msg =  "No connected target".to_string();
        transfer::send_channel_msg(task_info.channel_id, transfer::EchoLevel::RAW, msg).await?;
    } else if task_info.connect_key == "any" {
            hdc::info!("Wait for connected target any");
            let msg = "Wait for connected target any get ".to_string() +  target_list[0].as_str();
            transfer::send_channel_msg(task_info.channel_id, transfer::EchoLevel::RAW, msg).await?;
            transfer::TcpMap::end(task_info.channel_id).await;
    } else { // wait for special connectkey
            if target_list.iter().any(|connect_key| connect_key == &task_info.connect_key) {
                hdc::info!("Wait for connected target is {}", task_info.connect_key);
                let msg = "Wait for connected target is ".to_string() +  task_info.connect_key.as_str();
                transfer::send_channel_msg(task_info.channel_id, transfer::EchoLevel::RAW, msg).await?;
                transfer::TcpMap::end(task_info.channel_id).await; 
            } else {
                hdc::info!("No {} connected target ", task_info.connect_key);
                let msg =  "No connected target".to_string();
                transfer::send_channel_msg(task_info.channel_id, transfer::EchoLevel::RAW, msg).await?;
            }
    }
    Ok(())
}

async fn tcp_handle_deamon(
    mut rd: SplitReadHalf,
    session_id: u32,
    connect_key: String,
) -> io::Result<()> {
    loop {
        match transfer::tcp::unpack_task_message(&mut rd).await {
            Ok(task_message) => {
                // hdc::info!(
                //     "in tcp_handle_deamon, recv cmd: {:#?}, payload len: {}",
                //     task_message.command,
                //     task_message.payload.len(),
                // );
                if let Err(e) = session_task_dispatch(task_message, session_id).await {
                    hdc::error!("dispatch task failed: {}", e.to_string());
                }
            }
            Err(e) => {
                hdc::warn!("unpack task failed: {}", e.to_string());
                ConnectMap::remove(connect_key).await;
                return Err(e);
            }
        };
    }
}

async fn session_task_dispatch(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    match task_message.command {
        HdcCommand::KernelEchoRaw | HdcCommand::UnityBugreportData | HdcCommand::KernelEcho => {
            transfer::send_channel_data(task_message.channel_id, task_message.payload).await;
        }
        HdcCommand::KernelChannelClose => {
            session_channel_close(task_message, session_id).await?;
        }
        HdcCommand::AppBegin
        | HdcCommand::AppData
        | HdcCommand::AppFinish
        | HdcCommand::FileInit
        | HdcCommand::FileBegin
        | HdcCommand::FileData
        | HdcCommand::FileCheck
        | HdcCommand::FileFinish => {
            session_file_task(task_message, session_id).await?;
        }
        _ => {}
    }
    Ok(())
}

async fn session_file_task(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    match task_message.command {
        HdcCommand::AppBegin | HdcCommand::AppFinish => {
            let _ = host_app::command_dispatch(
                session_id,
                task_message.channel_id,
                task_message.command,
                &task_message.payload,
                task_message.payload.len() as u16,
            )
            .await;
            return Ok(());
        }
        HdcCommand::FileCheck | HdcCommand::FileInit => {
            if !FileTaskMap::exsit(session_id, task_message.channel_id).await {
                let mut task = HdcFile::new(session_id, task_message.channel_id);
                task.transfer.server_or_daemon = true;
                FileTaskMap::put(session_id, task_message.channel_id, task).await;
            }

            hdcfile::command_dispatch(
                session_id,
                task_message.channel_id,
                task_message.command,
                &task_message.payload,
                task_message.payload.len() as u16,
            )
            .await;
            return Ok(());
        }
        HdcCommand::FileBegin | HdcCommand::FileData | HdcCommand::FileFinish => {
            hdcfile::command_dispatch(
                session_id,
                task_message.channel_id,
                task_message.command,
                &task_message.payload,
                task_message.payload.len() as u16,
            )
            .await;
            return Ok(());
        }
        _ => {
            println!("other tasks");
        }
    }
    /* ActionType 未定义，临时屏蔽
    let channel_id = task_message.channel_id;
    let command = task_message.command;

    let opt = admin_session(ActionType::Query(session_id)).await;
    if opt.is_none() {
        admin_session(ActionType::Add(HdcSession::new(
            session_id,
            String::from(""),
            NodeType::Server,
            ConnectType::Tcp,
        )))
        .await;
    }
    let opt = admin_session(ActionType::Query(session_id)).await;

    let arc = opt.unwrap();
    let mut session = arc.lock().await;
    if let std::collections::hash_map::Entry::Vacant(e) = session.map_tasks.entry(channel_id) {
        match command {
            HdcCommand::AppBegin => {
                let mut task = HostAppTask::new(session_id, channel_id);
                task.transfer.server_or_daemon = true;
                e.insert(Arc::new(Mutex::new(task)));
            }
            HdcCommand::FileInit => {
                let mut task = HdcFile::new(session_id, channel_id);
                task.transfer.server_or_daemon = true;
                e.insert(Arc::new(Mutex::new(task)));
            }
            _ => {
                println!("other tasks");
            }
        }
    }
    let task = session.map_tasks.get(&channel_id).unwrap();
    let task_ = &mut task.lock().await;
    let cmd = task_message.payload;
    let _ = task_.command_dispatch(command, &cmd[..], cmd.len() as u16);
    */
    Ok(())
}

async fn session_channel_close(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    HostAppTaskMap::remove(session_id, task_message.channel_id).await;
    if task_message.payload[0] > 0 {
        let message = TaskMessage {
            channel_id: task_message.channel_id,
            command: HdcCommand::KernelChannelClose,
            payload: vec![task_message.payload[0] - 1],
        };
        transfer::put(session_id, message).await;
    }
    hdc::info!("recv channel close");
    transfer::TcpMap::end(task_message.channel_id).await;
    Ok(())
}

async fn check_server_task(task_info: TaskInfo) -> io::Result<()> {
    let payload = [
        u16::to_le_bytes(HdcCommand::KernelCheckServer as u16).as_slice(),
        get_version().as_bytes(),
    ]
    .concat();
    transfer::send_channel_data(task_info.channel_id, payload).await;
    Ok(())
}

#[allow(unused)]
#[derive(Default)]
enum ConnectStatus {
    #[default]
    Unknown = 0,
    Ready,
    Connected,
    Offline,
}

#[allow(unused)]
#[derive(Default)]
struct DaemonInfo {
    pub session_id: u32,
    pub conn_type: ConnectType,
    pub conn_status: ConnectStatus,
    pub dev_name: String,
    pub version: String,
}

type DaemonInfo_ = Arc<Mutex<DaemonInfo>>;
type ConnectMap_ = Arc<RwLock<HashMap<String, DaemonInfo_>>>;

pub struct ConnectMap {}
impl ConnectMap {
    fn get_instance() -> ConnectMap_ {
        static mut CONNECT_TYPE_MAP: Option<ConnectMap_> = None;
        unsafe {
            CONNECT_TYPE_MAP
                .get_or_insert_with(|| Arc::new(RwLock::new(HashMap::new())))
                .clone()
        }
    }

    async fn remove(connect_key: String) {
        let instance = Self::get_instance();
        let mut map = instance.write().await;
        map.remove(&connect_key);
    }

    async fn put(connect_key: String, daemon_info: DaemonInfo) {
        let instance = Self::get_instance();
        let mut map = instance.write().await;
        map.insert(connect_key, Arc::new(Mutex::new(daemon_info)));
    }

    async fn get(connect_key: String) -> Option<DaemonInfo_> {
        let instance = Self::get_instance();
        let map = instance.read().await;
        let key = if connect_key.as_str() == "any" && map.keys().len() == 1 {
            map.keys().last().unwrap()
        } else {
            &connect_key
        };
        map.get(key).cloned()
    }

    async fn get_list(is_full: bool) -> Vec<String> {
        let instance = Self::get_instance();
        let map = instance.read().await;
        let mut list = vec![];
        for (key, info) in map.iter() {
            if is_full {
                let mut output = vec![key.as_str()];
                let guard = info.lock().await;
                output.push(match guard.conn_type {
                    ConnectType::Tcp => "TCP",
                    ConnectType::Usb(_) => "USB",
                    ConnectType::Uart => "UART",
                    ConnectType::Bt => "BT",
                    ConnectType::HostUsb(_) => "HOSTUSB",
                    ConnectType::Bridge => "BRIDGE",
                });
                output.push(match guard.conn_status {
                    ConnectStatus::Connected => "Connected",
                    ConnectStatus::Ready => "Ready",
                    ConnectStatus::Offline => "Offline",
                    ConnectStatus::Unknown => "Unknown",
                });
                if guard.dev_name.is_empty() {
                    output.push("unknown...");
                } else {
                    let dev_name = guard.dev_name.as_str();
                    output.push(dev_name);
                };
                output.push("hdc");
                list.push(output.join("\t"));
            } else {
                list.push(key.to_owned());
            }
        }
        list
    }

    pub async fn get_session_id(connect_key: String) -> Option<u32> {
        let daemon_info = Self::get(connect_key).await?;
        let guard = daemon_info.lock().await;
        Some(guard.session_id)
    }
}

async fn get_valid_session_id(connect_key: String, channel_id: u32) -> io::Result<u32> {
    match ConnectMap::get_session_id(connect_key).await {
        Some(session_id) => Ok(session_id),
        None => {
            transfer::send_channel_msg(
                channel_id,
                transfer::EchoLevel::FAIL,
                "Not match target founded, check connect-key please".to_string(),
            )
            .await?;
            transfer::TcpMap::end(channel_id).await;
            Err(Error::new(ErrorKind::Other, "session not found"))
        }
    }
}
