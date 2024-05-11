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
use hdc::common::base::Base;
use hdc::common::filemanager::FileManager;
use hdc::common::hdctransfer::{self, HdcTransferBase};
use hdc::config;
use hdc::config::HdcCommand;
use hdc::config::TaskMessage;
use hdc::config::TRANSFER_FUNC_NAME;
use hdc::serializer::serialize::Serialization;
use hdc::transfer;
use hdc::transfer::EchoLevel;
use hdc::utils;
use std::collections::HashMap;
use std::io;
use std::path::PathBuf;
use std::sync::Arc;
use ylong_runtime::sync::Mutex;
#[cfg(feature = "host")]
extern crate ylong_runtime_static as ylong_runtime;

pub struct HostAppTask {
    pub transfer: HdcTransferBase,
    pub printed_msg_len: usize,
}

impl HostAppTask {
    ///  complie failed ,associated function `new` is never used
    pub fn new(_session_id: u32, _channel_id: u32) -> Self {
        Self {
            transfer: HdcTransferBase::new(_session_id, _channel_id),
            printed_msg_len: 0,
        }
    }
}

type HostAppTask_ = Arc<Mutex<HostAppTask>>;
type HostAppTaskMap_ = Arc<Mutex<HashMap<(u32, u32), HostAppTask_>>>;

pub struct HostAppTaskMap {}
impl HostAppTaskMap {
    fn get_instance() -> HostAppTaskMap_ {
        static mut HOSTAPPTASKMAP: Option<HostAppTaskMap_> = None;
        unsafe {
            HOSTAPPTASKMAP
                .get_or_insert_with(|| Arc::new(Mutex::new(HashMap::new())))
                .clone()
        }
    }

    pub async fn put(session_id: u32, channel_id: u32, host_app_task: HostAppTask) {
        let arc_map = Self::get_instance();
        let mut map = arc_map.lock().await;
        map.insert(
            (session_id, channel_id),
            Arc::new(Mutex::new(host_app_task)),
        );
    }

    pub async fn exist(session_id: u32, channel_id: u32) -> Result<bool, ()> {
        let arc_map = Self::get_instance();
        let map = arc_map.lock().await;
        Ok(map.contains_key(&(session_id, channel_id)))
    }

    pub async fn remove(session_id: u32, channel_id: u32) -> Option<HostAppTask_> {
        let arc_map = Self::get_instance();
        let mut map = arc_map.lock().await;
        map.remove(&(session_id, channel_id))
    }

    pub async fn get(session_id: u32, channel_id: u32) -> HostAppTask_ {
        let arc_map = Self::get_instance();
        let map = arc_map.lock().await;
        let arc_task = map.get(&(session_id, channel_id)).unwrap();
        arc_task.clone()
    }
}

pub async fn send_to_client(channel_id: u32, level: EchoLevel, message: String) -> io::Result<()> {
    transfer::send_channel_msg(channel_id, level, message).await
}

pub async fn echo_client(channel_id: u32, message: String) -> io::Result<()> {
    send_to_client(channel_id, EchoLevel::INFO, message).await
}

async fn check_install_continue(
    session_id: u32,
    channel_id: u32,
    mode_type: config::AppModeType,
    str: String,
) -> bool {
    let mut _mode_desc = String::from("");
    match mode_type {
        config::AppModeType::Install => _mode_desc = String::from("App install"),
        config::AppModeType::UnInstall => _mode_desc = String::from("App uninstall"),
    }
    let arc_task = HostAppTaskMap::get(session_id, channel_id).await;
    let mut task = arc_task.lock().await;
    let msg = str[task.printed_msg_len..].to_owned();
    let message = format!(
        "{} path:{}, queuesize:{}, msg:{}",
        _mode_desc,
        task.transfer.local_path.clone(),
        task.transfer.task_queue.len(),
        msg
    );
    task.printed_msg_len = str.len();
    let _ = echo_client(channel_id, message).await;
    if task.transfer.task_queue.is_empty() {
        let _ = echo_client(channel_id, String::from("AppMod finish")).await;
        task_finish(session_id, channel_id).await;
        hdctransfer::close_channel(channel_id).await;
        return false;
    }
    drop(task);
    install_single(session_id, channel_id).await;
    put_app_check(session_id, channel_id).await;
    true
}

async fn do_app_uninstall(session_id: u32, channel_id: u32, _payload: &[u8]) {
    let app_uninstall_message = TaskMessage {
        channel_id,
        command: HdcCommand::AppUninstall,
        payload: _payload.to_vec(),
    };
    transfer::put(session_id, app_uninstall_message).await;
}

async fn do_app_finish(session_id: u32, channel_id: u32, _payload: &[u8]) -> bool {
    let mode = config::AppModeType::try_from(_payload[0]);
    if let Ok(mode_type) = mode {
        let str = String::from_utf8(_payload[2..].to_vec()).unwrap();
        return check_install_continue(session_id, channel_id, mode_type, str).await;
    }
    false
}

pub fn get_sub_app_files_resurively(
    channel_id: u32,
    dir_path: &PathBuf,
) -> Result<Vec<String>, Box<dyn std::error::Error>> {
    let mut result = Vec::new();
    let dir = match std::fs::read_dir(dir_path) {
        Ok(dir) => dir,
        Err(e) => {
            let message = format!("App install path:{}, msg:{}", dir_path.display(), e);
            ylong_runtime::block_on(async {
                let _ = send_to_client(channel_id, EchoLevel::FAIL, message).await;
            });
            return Ok(result);
        }
    };
    for entry in dir {
        let path = match entry {
            Ok(entry) => entry.path(),
            Err(e) => {
                let message = format!("App install path:{}, msg:{}", dir_path.display(), e);
                ylong_runtime::block_on(async {
                    let _ = send_to_client(channel_id, EchoLevel::FAIL, message).await;
                });
                continue;
            }
        };

        let metadata = match std::fs::metadata(path.clone()) {
            Ok(metadata) => metadata,
            Err(_) => {
                continue;
            }
        };

        if metadata.is_file() {
            let p = path.display().to_string();
            if p.ends_with(".hap") || p.ends_with(".hsp") {
                result.push(p.clone());
            }
            continue;
        }

        if metadata.is_dir() {
            let mut sub_list = get_sub_app_files_resurively(channel_id, &path).unwrap();
            result.append(&mut sub_list);
        }
    }
    result.sort();
    Ok(result)
}

async fn task_finish(session_id: u32, channel_id: u32) {
    hdctransfer::transfer_task_finish(channel_id, session_id).await
}

async fn put_app_check(session_id: u32, channel_id: u32) {
    let arc_task = HostAppTaskMap::get(session_id, channel_id).await;
    let task = arc_task.lock().await;
    let file_check_message = TaskMessage {
        channel_id,
        command: HdcCommand::AppCheck,
        payload: task.transfer.transfer_config.serialize(),
    };
    transfer::put(session_id, file_check_message).await
}

async fn install_single(session_id: u32, channel_id: u32) {
    let arc_task = HostAppTaskMap::get(session_id, channel_id).await;
    let mut task = arc_task.lock().await;
    task.transfer.local_path = task.transfer.task_queue.pop().unwrap();
    let local_path = task.transfer.local_path.clone();
    let mut file_manager = FileManager::new(local_path.clone());
    let (open_result, error_msg) = file_manager.open();
    if open_result {
        let file_size = file_manager.file_size();
        task.transfer.transfer_config.file_size = file_size;
        task.transfer.file_size = file_size;
        task.transfer.transfer_config.optional_name = utils::get_pseudo_random_u32().to_string();
        if let Some(index) = local_path.rfind('.') {
            let str = local_path.as_str();
            task.transfer
                .transfer_config
                .optional_name
                .push_str(&str[index..]);
        }
        // if config.hold_timestamp {}
        task.transfer.transfer_config.path = task.transfer.remote_path.clone();
    } else {
        println!("other command {:#?}", error_msg);
        task_finish(session_id, channel_id).await;
    }
}

async fn init_install(session_id: u32, channel_id: u32, command: &String) -> bool {
    let (argv, argc) = Base::split_command_to_args(command);
    if argc < 1 {
        return false;
    }

    let arc_task = HostAppTaskMap::get(session_id, channel_id).await;
    let mut task = arc_task.lock().await;
    let mut i = 1usize;
    let mut options = String::from("");
    while i < argc as usize {
        if argv[i] == "-cwd" {
            if i + 1 < argc as usize {
                task.transfer.transfer_config.client_cwd = argv[i + 1].clone();
                i += 1;
            }
        } else if argv[i].starts_with('-') {
            if !options.is_empty() {
                options.push(' ');
            }
            options.push_str(&argv[i].clone());
        } else {
            let mut path = argv[i].clone() as String;
            path = Base::extract_relative_path(
                &task.transfer.transfer_config.client_cwd,
                path.as_str(),
            );
            if path.ends_with(".hap") || path.ends_with(".hsp") {
                task.transfer.task_queue.push(path.clone());
            } else {
                let mut queue =
                    get_sub_app_files_resurively(channel_id, &PathBuf::from(path)).unwrap();
                task.transfer.task_queue.append(&mut queue);
            }
        }
        i += 1;
    }

    if task.transfer.task_queue.is_empty() {
        return false;
    }

    task.transfer.transfer_config.options = options.clone();
    task.transfer.transfer_config.function_name = TRANSFER_FUNC_NAME.to_string();
    task.transfer.is_master = true;
    drop(task);
    install_single(session_id, channel_id).await;

    true
}

pub async fn command_dispatch(
    session_id: u32,
    channel_id: u32,
    _command: HdcCommand,
    _payload: &[u8],
    _payload_size: u16,
) -> Result<bool, &str> {
    match _command {
        HdcCommand::AppInit => {
            let s = String::from_utf8(_payload.to_vec());
            match s {
                Ok(str) => {
                    if !init_install(session_id, channel_id, &str).await {
                        let message = "Not any installation package was found";
                        let _ =
                            send_to_client(channel_id, EchoLevel::FAIL, message.to_owned()).await;
                        transfer::TcpMap::end(channel_id).await;
                        return Ok(false);
                    }
                    put_app_check(session_id, channel_id).await
                }
                Err(e) => {
                    println!("error {}", e);
                }
            }
        }
        HdcCommand::AppBegin => {
            let arc_task = HostAppTaskMap::get(session_id, channel_id).await;
            let task = arc_task.lock().await;
            hdctransfer::transfer_begin(&task.transfer, HdcCommand::AppData).await;
        }
        HdcCommand::AppUninstall => {
            let s = String::from_utf8(_payload.to_vec());
            let mut options = String::from("");
            match s {
                Ok(str) => {
                    let (argv, argc) = Base::split_command_to_args(&str);
                    if argc < 1 {
                        return Ok(false);
                    }
                    options = argv[1..].join(" ");
                }
                Err(e) => {
                    println!("error {}", e);
                }
            }
            do_app_uninstall(session_id, channel_id, options.as_bytes()).await;
        }
        HdcCommand::AppFinish => {
            do_app_finish(session_id, channel_id, _payload).await;
        }
        _ => {
            println!("other command");
        }
    }
    Ok(true)
}
