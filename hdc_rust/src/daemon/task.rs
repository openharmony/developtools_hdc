//! task
#![allow(missing_docs)]

use crate::auth;

use super::daemon_app::DaemonAppTask;
use super::daemon_unity::DaemonUnityTask;
use super::shell::{PtyMap, PtyTask};

use hdc::common::hdcfile::HdcFile;
use hdc::common::hsession::*;
use hdc::config::*;
use hdc::transfer;
use hdc::utils::hdc_log::*;

use std::io::{self, Error, ErrorKind};
use std::sync::Arc;

use ylong_runtime::sync::Mutex;

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
    // file & install task
    if let Some(arc) = admin_session(ActionType::Query(session_id)).await {
        let mut session = arc.lock().await;
        if let Some(task) = session.map_tasks.remove(&channel_id) {
            let guard = &mut task.lock().await;
            guard.stop_task();
        }
    }
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

async fn daemon_forward_task(task_message: TaskMessage, _session_id: u32) -> io::Result<()> {
    hdc::warn!("get shell data payload: {:#?}", task_message.payload);
    hdc::warn!("get command: {}", task_message.command as u32);
    Ok(())
}

async fn daemon_file_task(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    let opt = admin_session(ActionType::Query(session_id)).await;
    if opt.is_none() {
        admin_session(ActionType::Add(HdcSession::new(
            session_id,
            String::from(""),
            NodeType::Daemon,
            ConnectType::Tcp,
        )))
        .await;
    }
    let opt = admin_session(ActionType::Query(session_id)).await;

    let arc = opt.unwrap();
    let mut session = arc.lock().await;
    if let std::collections::hash_map::Entry::Vacant(e) =
        session.map_tasks.entry(task_message.channel_id)
    {
        match task_message.command {
            HdcCommand::AppCheck | HdcCommand::AppUninstall => {
                let task = DaemonAppTask::new(session_id, task_message.channel_id);
                e.insert(Arc::new(Mutex::new(task)));
            }
            HdcCommand::FileCheck | HdcCommand::FileInit => {
                let mut task = HdcFile::new(session_id, task_message.channel_id);
                task.transfer.server_or_daemon = false;
                e.insert(Arc::new(Mutex::new(task)));
            }
            HdcCommand::UnityRunmode
            | HdcCommand::UnityRootrun
            | HdcCommand::JdwpList
            | HdcCommand::JdwpTrack => {
                let task = DaemonUnityTask::new(session_id, task_message.channel_id);
                e.insert(Arc::new(Mutex::new(task)));
            }
            _ => {
                println!("other tasks");
            }
        }
    }
    let task = session.map_tasks.get(&task_message.channel_id).unwrap();
    let task_ = &mut task.lock().await;
    let _ = task_.command_dispatch(
        task_message.command,
        &task_message.payload,
        task_message.payload.len() as u16,
    );

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

pub async fn dispatch_task(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
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
        | HdcCommand::AppUninstall => {
            hdc::debug!("FileCheck:{}", task_message.command as u32);
            daemon_file_task(task_message, session_id).await
        }

        HdcCommand::ForwardInit | HdcCommand::ForwardCheck => {
            hdc::debug!("ForwardInit");
            daemon_forward_task(task_message, session_id).await
        }

        HdcCommand::UnityRunmode
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
