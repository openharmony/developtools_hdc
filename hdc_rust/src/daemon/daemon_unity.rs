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
use super::mount;
use super::shell::PtyMap;
use crate::jdwp::Jdwp;
use hdc::common::hdctransfer;
use hdc::config::{self, HdcCommand, MessageLevel};
use libc::sync;
use crate::sys_para::{*};
use crate::utils::hdc_log::*;

extern "C" {
    fn Restart();
}

async fn hdc_restart() {
    hdc::info!("Mode changed, hdc daemon restart!");
    unsafe {
        Restart();
    }
}

async fn echo_client(session_id: u32, channel_id: u32, message: &str, level: MessageLevel) {
    println!("echo_client for session {} channel {}: {}", session_id, channel_id, message);
    hdctransfer::echo_client(session_id, channel_id, message.as_bytes().to_vec(), level).await;
}

async fn echo_device_mode_result(session_id: u32, channel_id: u32, result: bool, message: &str) {
    if result {
        echo_client(session_id, channel_id, "Set device run mode successful.", MessageLevel::Ok).await;
    } else {
        let msg = format!("Set device run mode failed: {}", message);
        echo_client(session_id, channel_id, msg.as_str(), MessageLevel::Fail).await;
    }
    task_finish(session_id, channel_id).await;
}

async fn echo_reboot_result(session_id: u32, channel_id: u32, result: bool, message: &str) {
    if result {
        echo_client(session_id, channel_id, "Reboot successful.", MessageLevel::Ok).await;
    } else {
        let msg = format!("Reboot failed: {}", message);
        echo_client(session_id, channel_id, msg.as_str(), MessageLevel::Fail).await;
    }
    task_finish(session_id, channel_id).await;
}

async fn echo_root_run_mode_result(
    session_id: u32,
    channel_id: u32,
    result: bool,
    message: &str,
) {
    if result {
        let msg = format!("Set {} run mode successful.", message);
        echo_client(session_id, channel_id, msg.as_str(), MessageLevel::Ok).await;
    } else {
        let msg = format!("Set {} run mode failed.", message);
        echo_client(session_id, channel_id, msg.as_str(), MessageLevel::Fail).await;
    }
    task_finish(session_id, channel_id).await;
}

async fn set_root_run_enable(session_id: u32, channel_id: u32, root: bool) {
    let root_flag = if root { "0" } else { "1" };
    let mode_msg = if root { "sh" } else { "root" };
    let result = set_dev_item(config::ENV_ROOT_RUN_MODE, root_flag);
    echo_root_run_mode_result(session_id, channel_id, result, mode_msg).await;
    hdc::info!(
        "set_root_run_enable: session_id: {}, channel_id: {}, root: {}, result: {}",
        session_id,
        channel_id,
        root,
        result
    );
    if result {
        PtyMap::clear(session_id).await;
        std::process::exit(0);
    }
}

async fn set_root_run(session_id: u32, channel_id: u32, _payload: &[u8]) {
    let (ret, debug_able) = get_dev_item(config::ENV_DEBUGGABLE, "_");
    if !ret || debug_able.trim() != "1" {
        hdc::info!("get debuggable failed");
        echo_client(
            session_id,
            channel_id,
            "Cannot set root run mode in undebuggable version.",
            MessageLevel::Fail
        )
        .await;
        task_finish(session_id, channel_id).await;
        return;
    }

    if _payload.is_empty() {
        set_root_run_enable(session_id, channel_id, false).await;
    } else if _payload == [b'r'] {
        set_root_run_enable(session_id, channel_id, true).await;
    } else {
        echo_root_run_mode_result(
            session_id,
            channel_id,
            false,
            "Unknown command",
        )
        .await;
    }
}

async fn reboot_device(session_id: u32, channel_id: u32, _payload: &[u8]) {
    mount::remount_device();
    unsafe {
        sync();
    };

    let param = String::from_utf8(_payload.to_vec()).unwrap();
    let mut cmd = String::from("reboot");
    if !param.is_empty() {
        cmd.push(',');
        cmd.push_str(param.as_str());
    }
    let cmd = cmd.trim();
    let result = set_dev_item(config::ENV_STARTUP, cmd);
    echo_reboot_result(session_id, channel_id, result, cmd).await;
}

async fn remount_device(session_id: u32, channel_id: u32) {
    unsafe {
        if libc::getuid() !=0 {
            echo_client(session_id, channel_id, "Operate need running as root", MessageLevel::Fail).await;
            task_finish(session_id, channel_id).await;
            return;
        }
    }
    let ret = mount::remount_device();
    if ret {
        echo_client(session_id, channel_id, "Mount finish", MessageLevel::Ok).await;
    } else {
        echo_client(session_id, channel_id, "Mount failed", MessageLevel::Fail).await;
    }
    task_finish(session_id, channel_id).await;
}

async fn set_device_mode(session_id: u32, channel_id: u32, _payload: &[u8]) {
    let param = String::from_utf8(_payload.to_vec()).unwrap();
    match param.as_str() {
        config::MODE_USB => {
            let result = set_dev_item(config::ENV_HDC_MODE, config::MODE_USB);
            echo_device_mode_result(session_id, channel_id, result, config::MODE_USB).await;
            if result {
                PtyMap::clear(session_id).await;
                hdc_restart().await
            }
        }
        str if str.starts_with(config::PREFIX_PORT) => {
            let result = set_dev_item(config::ENV_HDC_MODE, config::MODE_TCP);
            if !result {
                echo_device_mode_result(session_id, channel_id, result, config::MODE_TCP).await;
                return;
            }

            let port = &str[config::PREFIX_PORT.len()..];
            let port =
                port.trim_end_matches(|c: char| c.is_ascii_control() || c.is_ascii_whitespace());
            let result = set_dev_item(config::ENV_HOST_PORT, port);
            echo_device_mode_result(session_id, channel_id, result, config::ENV_HOST_PORT).await;
            if result {
                PtyMap::clear(session_id).await;
                hdc_restart().await
            }
        }
        _ => {
            echo_device_mode_result(
                session_id,
                channel_id,
                false,
                "Unknown command",
            )
            .await;
        }
    }
}

async fn do_jdwp_list(session_id: u32, channel_id: u32) {
    println!("do_jdwp_list");
    let jdwp = Jdwp::get_instance().clone();
    let process_list = jdwp.get_process_list().await;
    if process_list.is_empty() {
        echo_client(session_id, channel_id, "[Empty]", MessageLevel::Ok).await;
    } else {
        echo_client(session_id, channel_id, process_list.as_str(), MessageLevel::Ok).await;
    }
    task_finish(session_id, channel_id).await;
}

async fn do_jdwp_track(session_id: u32, channel_id: u32, payload: &[u8]) {
    let mut debug_or_release = true;
    if !payload.is_empty() && payload[0] == b'p' {
        debug_or_release = false;
    }
    println!("do_jdwp_track");
    let jdwp = Jdwp::get_instance().clone();
    jdwp.add_tracker(channel_id, session_id, debug_or_release)
        .await;
}

pub async fn command_dispatch(
    session_id: u32,
    channel_id: u32,
    _command: HdcCommand,
    _payload: &[u8],
    _payload_size: u16,
) -> bool {
    println!("DaemonUnityTask: command:{:#?}", _command);
    match _command {
        HdcCommand::UnityReboot => {
            reboot_device(session_id, channel_id, _payload).await;
        }
        HdcCommand::UnityRunmode => {
            set_device_mode(session_id, channel_id, _payload).await;
        }
        HdcCommand::UnityRootrun => {
            set_root_run(session_id, channel_id, _payload).await;
        }
        HdcCommand::JdwpList => {
            do_jdwp_list(session_id, channel_id).await;
        }
        HdcCommand::JdwpTrack => {
            do_jdwp_track(session_id, channel_id, _payload).await;
        }
        HdcCommand::UnityRemount => {
            remount_device(session_id, channel_id).await;
        }
        _ => {
            println!("other command");
        }
    }
    true
}

async fn task_finish(session_id: u32, channel_id: u32) {
    hdctransfer::transfer_task_finish(channel_id, session_id).await;
}
