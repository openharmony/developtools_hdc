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
//! task
#![allow(missing_docs)]

use crate::{auth, daemon_unity};

use super::shell::{PtyMap, PtyTask};

use super::daemon_app::{self, AppTaskMap, DaemonAppTask};
use crate::utils::hdc_log::*;
use hdc::common::forward::{self, ForwardTaskMap, HdcForward};
use hdc::common::hdcfile::{self, FileTaskMap, HdcFile};
use hdc::config::*;
use hdc::transfer;

use std::io::{self, Error, ErrorKind};

async fn daemon_shell_task(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    match task_message.command {
        HdcCommand::ShellInit => {
            let pty_task = PtyTask::new(
                session_id,
                task_message.channel_id,
                None,
                HdcCommand::KernelEchoRaw,
            );
            PtyMap::put(task_message.channel_id, pty_task).await;
        }
        HdcCommand::UnityExecute => {
            let cmd = String::from_utf8(task_message.payload).unwrap();
            let pty_task = PtyTask::new(
                session_id,
                task_message.channel_id,
                Some(cmd),
                HdcCommand::KernelEchoRaw,
            );
            PtyMap::put(task_message.channel_id, pty_task).await;
        }
        _ => {
            hdc::debug!("get shell data payload: {:#?}", task_message.payload);
            let channel_id = task_message.channel_id;
            if let Some(pty_task) = PtyMap::get(channel_id).await {
                hdc::debug!("get shell data pty");
                for byte in task_message.payload.iter() {
                    let _ = &pty_task.tx.send(*byte).await;

                    if *byte == 0x4_u8 {
                        PtyMap::del(channel_id).await;
                        break;
                    }
                }
                return Ok(());
            } else {
                return Err(Error::new(ErrorKind::Other, "invalid channel id"));
            }
        }
    }
    Ok(())
}

async fn remove_task(session_id: u32, channel_id: u32) {
    AppTaskMap::remove(session_id, channel_id).await;
    FileTaskMap::remove(session_id, channel_id).await;
    // shell & hilog task
    if let Some(pty_task) = PtyMap::get(channel_id).await {
        let _ = &pty_task.tx.send(0x04_u8).await;
        PtyMap::del(channel_id).await;
    }
}

async fn daemon_channel_close(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    // task stop:
    remove_task(session_id, task_message.channel_id).await;

    if task_message.payload[0] > 0 {
        let message = TaskMessage {
            channel_id: task_message.channel_id,
            command: HdcCommand::KernelChannelClose,
            payload: vec![task_message.payload[0] - 1],
        };
        transfer::put(session_id, message).await;
    }
    Ok(())
}

async fn daemon_file_task(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    match task_message.command {
        HdcCommand::AppCheck | HdcCommand::AppUninstall => {
            if !AppTaskMap::exsit(session_id, task_message.channel_id).await {
                let task = DaemonAppTask::new(session_id, task_message.channel_id);
                AppTaskMap::put(session_id, task_message.channel_id, task).await;
            }
            daemon_app::command_dispatch(
                session_id,
                task_message.channel_id,
                task_message.command,
                &task_message.payload,
                task_message.payload.len() as u16,
            )
            .await;
            return Ok(());
        }
        HdcCommand::AppBegin | HdcCommand::AppData => {
            daemon_app::command_dispatch(
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
                task.transfer.server_or_daemon = false;
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
        HdcCommand::ForwardInit | HdcCommand::ForwardCheck => {
            let mut task = HdcForward::new(session_id, task_message.channel_id);
            task.transfer.server_or_daemon = false;
            ForwardTaskMap::update(session_id, task_message.channel_id, task).await;
            forward::command_dispatch(
                session_id,
                task_message.channel_id,
                task_message.command,
                &task_message.payload,
                task_message.payload.len() as u16,
            )
            .await;
            return Ok(());
        }
        HdcCommand::ForwardCheckResult
        | HdcCommand::ForwardActiveSlave
        | HdcCommand::ForwardActiveMaster
        | HdcCommand::ForwardData
        | HdcCommand::ForwardFreeContext => {
            forward::command_dispatch(
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
        HdcCommand::UnityRunmode
        | HdcCommand::UnityReboot
        | HdcCommand::UnityRemount
        | HdcCommand::UnityRootrun
        | HdcCommand::JdwpList
        | HdcCommand::JdwpTrack => {
            daemon_unity::command_dispatch(
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

    Ok(())
}

async fn daemon_hilog_task(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    let cmd = if task_message.payload.len() == 1 && task_message.payload[0] == 104 {
        // payload is 'h'
        "hilog -h"
    } else {
        // blank or unknown payload, ignore
        "hilog"
    }
    .to_string();
    let pty_task = PtyTask::new(
        session_id,
        task_message.channel_id,
        Some(cmd),
        HdcCommand::KernelEchoRaw,
    );
    PtyMap::put(task_message.channel_id, pty_task).await;
    Ok(())
}

async fn daemon_bug_report_task(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    let pty_task = PtyTask::new(
        session_id,
        task_message.channel_id,
        Some("hidumper".to_string()),
        HdcCommand::UnityBugreportData,
    );
    PtyMap::put(task_message.channel_id, pty_task).await;
    Ok(())
}

// fn get_control_permission(param: &str) -> bool {
//     let (_, control_ret) = hdc::utils::get_dev_item(param);
//     if control_ret.starts_with('0') {
//         return false;
//     }
//     true
// }

// fn check_control(command: HdcCommand) -> bool {
//     let mut control_param = "";
//     match command {
//         HdcCommand::UnityRunmode
//         | HdcCommand::UnityReboot
//         | HdcCommand::UnityRemount
//         | HdcCommand::UnityRootrun
// 	    | HdcCommand::ShellInit
// 	    | HdcCommand::ShellData
// 	    | HdcCommand::UnityExecute
//         | HdcCommand::UnityHilog
// 	    | HdcCommand::UnityBugreportInit
// 	    | HdcCommand::JdwpList
//         | HdcCommand::JdwpTrack => {
//             control_param = ENV_SHELL_CONTROL;
//         }
//         HdcCommand::FileInit
//         | HdcCommand::FileCheck
//         | HdcCommand::FileData
//         | HdcCommand::FileBegin
//         | HdcCommand::FileFinish
//         | HdcCommand::AppCheck
//         | HdcCommand::AppData
//         | HdcCommand::AppUninstall => {
//             control_param = ENV_FILE_CONTROL;
//         }
//         HdcCommand::ForwardInit
//         | HdcCommand::ForwardCheck
//         | HdcCommand::ForwardCheckResult
//         | HdcCommand::ForwardActiveSlave
//         | HdcCommand::ForwardActiveMaster
//         | HdcCommand::ForwardData
//         | HdcCommand::ForwardFreeContext => {
//             control_param = ENV_FPORT_CONTROL;
//         }
//         _ => {}
//     }
//     // (_, run_debug) = hdc::utils::get_dev_item(param);
//     if !control_param.is_empty() && !get_control_permission(control_param) {
//         return false;
//     }
//     true
// }

pub async fn dispatch_task(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    let cmd = task_message.command;
    let handshake_cmd = cmd == HdcCommand::KernelHandshake;
    let auth_ok = auth::AuthStatusMap::get(session_id).await == auth::AuthStatus::Ok;

    if !auth_ok && !handshake_cmd {
        hdc::error!("auth status is nok, cannt accept cmd: {}", cmd as u32);
        transfer::put(session_id,
            TaskMessage {
                channel_id: task_message.channel_id,
                command: HdcCommand::KernelChannelClose,
                payload: vec![0],
            },
        ).await;
        return Err(Error::new(
            ErrorKind::Other,
            format!("auth status is nok, cannt accept cmd: {}", cmd as u32),
        ));
    }
    // if !check_control(task_message.command) {
    //     hdc::common::hdctransfer::echo_client(session_id, task_message.channel_id, format!("check_permission param false: {}", task_message.command as u32).into_bytes()).await;
    //     hdc::common::hdctransfer::transfer_task_finish(task_message.channel_id, session_id).await;
    //     hdc::debug!("check_permission param false: {}", task_message.command as u32);

    //     return Ok(());
    // }
    match task_message.command {
        HdcCommand::KernelHandshake => {
            hdc::debug!("KernelHandshake");
            auth::handshake_task(task_message, session_id).await
        }
        HdcCommand::UnityHilog => {
            hdc::debug!("UnityHilog");
            daemon_hilog_task(task_message, session_id).await
        }
        HdcCommand::UnityBugreportInit => {
            hdc::debug!("UnityBugreportInit");
            daemon_bug_report_task(task_message, session_id).await
        }
        HdcCommand::ShellInit | HdcCommand::ShellData | HdcCommand::UnityExecute => {
            hdc::debug!("Shell: {:#?}", task_message.command);
            daemon_shell_task(task_message, session_id).await
        }
        HdcCommand::KernelChannelClose => {
            hdc::debug!("KernelChannelClose");
            daemon_channel_close(task_message, session_id).await
        }
        HdcCommand::FileInit
        | HdcCommand::FileCheck
        | HdcCommand::FileData
        | HdcCommand::FileBegin
        | HdcCommand::FileFinish
        | HdcCommand::AppCheck
        | HdcCommand::AppData
        | HdcCommand::AppUninstall
        | HdcCommand::ForwardInit
        | HdcCommand::ForwardCheck
        | HdcCommand::ForwardCheckResult
        | HdcCommand::ForwardActiveSlave
        | HdcCommand::ForwardActiveMaster
        | HdcCommand::ForwardData
        | HdcCommand::ForwardFreeContext => {
            hdc::debug!("FileCheck:{}", task_message.command as u32);
            daemon_file_task(task_message, session_id).await
        }
        HdcCommand::UnityRunmode
        | HdcCommand::UnityReboot
        | HdcCommand::UnityRemount
        | HdcCommand::UnityRootrun
        | HdcCommand::JdwpList
        | HdcCommand::JdwpTrack => {
            hdc::debug!("unity command: {:#?}", task_message.command);
            daemon_file_task(task_message, session_id).await
        }
        _ => Err(Error::new(
            ErrorKind::Other,
            format!("unknown command: {}", task_message.command as u32),
        )),
    }
}
