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
use hdc::common::hdcfile;
use hdc::common::hdctransfer::{self, HdcTransferBase};
use hdc::config::HdcCommand;
use hdc::config::TaskMessage;
use hdc::config::TRANSFER_FUNC_NAME;
use hdc::config::{self, INSTALL_TAR_MAX_CNT};
use hdc::serializer::serialize::Serialization;
use hdc::transfer;
use hdc::transfer::EchoLevel;
use hdc::utils;
use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::Arc;
use ylong_runtime::sync::Mutex;
#[cfg(feature = "host")]
extern crate ylong_runtime_static as ylong_runtime;
use hdc::tar::compress::Compress;

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

    pub async fn get(session_id: u32, channel_id: u32) -> Option<HostAppTask_> {
        let arc_map = Self::get_instance();
        let map = arc_map.lock().await;
        let Some(arc_task) = map.get(&(session_id, channel_id)) else {
            return None;
        };
        Some(arc_task.clone())
    }
}

async fn check_install_continue(
    session_id: u32,
    channel_id: u32,
    mode_type: config::AppModeType,
    str: String,
) -> bool {
    let mode_desc = match mode_type {
        config::AppModeType::Install => String::from("App install"),
        config::AppModeType::UnInstall => String::from("App uninstall"),
    };
    let Some(arc_task) = HostAppTaskMap::get(session_id, channel_id).await else {
        hdc::error!("Get host app task failed");
        return false;
    };
    let mut task = arc_task.lock().await;
    let msg = str[task.printed_msg_len..].to_owned();
    let local_path = if !task.transfer.local_tar_raw_path.is_empty() {
        &task.transfer.local_tar_raw_path
    } else {
        &task.transfer.local_path
    };
    let message =
        format!("{} path:{}, queuesize:{}, msg:{}", mode_desc, local_path, task.transfer.task_queue.len(), msg);
    hdc::info!("{message}");
    task.printed_msg_len = str.len();
    let _ = transfer::send_channel_msg(channel_id, EchoLevel::INFO, message).await;
    if task.transfer.task_queue.is_empty() {
        let _ = transfer::send_channel_msg(channel_id, EchoLevel::INFO, String::from("AppMod finish")).await;
        task_finish(session_id, channel_id).await;
        hdctransfer::close_channel(channel_id).await;
        return false;
    }
    drop(task);
    if let Err(err_msg) = install_single(session_id, channel_id).await {
        let _ = transfer::send_channel_msg(channel_id, EchoLevel::FAIL, err_msg).await;
        task_finish(session_id, channel_id).await;
        return false;
    }
    put_app_check(session_id, channel_id).await;
    true
}

async fn do_app_uninstall(session_id: u32, channel_id: u32, payload: Vec<u8>) {
    hdc::info!("send HdcCommand::AppUninstall");
    let app_uninstall_message = TaskMessage { channel_id, command: HdcCommand::AppUninstall, payload };
    transfer::put(session_id, app_uninstall_message).await;
}

async fn do_app_finish(session_id: u32, channel_id: u32, payload: &[u8]) -> bool {
    let mode = config::AppModeType::try_from(payload[0]);
    if let Ok(mode_type) = mode {
        let str = match String::from_utf8(payload[2..].to_vec()) {
            Ok(str) => str,
            Err(err) => {
                hdc::error!("do_app_finish from_utf8 error, {err}");
                return false;
            }
        };
        return check_install_continue(session_id, channel_id, mode_type, str).await;
    }
    false
}

fn dir_to_tar(dir_path: PathBuf) -> Result<String, String> {
    let mut compress = Compress::new();
    compress.updata_prefix(dir_path.clone());
    if let Err(err) = compress.add_path(&dir_path) {
        return Err(format!("Package as tar and add path error, {err}"));
    }
    compress.updata_max_count(INSTALL_TAR_MAX_CNT);

    let tar_name = utils::get_pseudo_random_u32().to_string() + ".tar";
    let tar_path = std::env::temp_dir().join(tar_name);
    match compress.compress(tar_path.clone()) {
        Ok(_) => Ok(tar_path.display().to_string()),
        Err(err) => Err(format!("compress {} fial, {}", tar_path.display(), err)),
    }
}

async fn task_finish(session_id: u32, channel_id: u32) {
    HostAppTaskMap::remove(session_id, channel_id).await;
    hdctransfer::transfer_task_finish(channel_id, session_id).await
}

async fn put_app_check(session_id: u32, channel_id: u32) {
    let Some(arc_task) = HostAppTaskMap::get(session_id, channel_id).await else {
        hdc::error!("Get host app task failed");
        return;
    };
    let task = arc_task.lock().await;
    hdc::info!("send HdcCommand::AppCheck");
    let file_check_message = TaskMessage {
        channel_id,
        command: HdcCommand::AppCheck,
        payload: task.transfer.transfer_config.serialize(),
    };
    transfer::put(session_id, file_check_message).await
}

async fn install_single(session_id: u32, channel_id: u32) -> Result<(), String> {
    let Some(arc_task) = HostAppTaskMap::get(session_id, channel_id).await else {
        hdc::error!("Get host app task failed");
        return Err("Internal error, Pls try again".to_owned());
    };
    let mut task = arc_task.lock().await;
    match task.transfer.task_queue.pop() {
        Some(loc_path) => {
            let loc_pathbuff = PathBuf::from(loc_path.clone());
            if loc_pathbuff.is_file() {
                task.transfer.local_path = loc_path;
                task.transfer.local_tar_raw_path = String::new();
            } else if loc_pathbuff.is_dir() {
                match dir_to_tar(loc_pathbuff) {
                    Ok(tar_file) => {
                        hdc::info!("dir_to_tar success, path = {}", tar_file);
                        task.transfer.local_path = tar_file;
                        task.transfer.local_tar_raw_path = loc_path;
                    }
                    Err(err) => {
                        hdc::error!("{}", err);
                        return Err("Folder packaging failed".to_owned());
                    }
                }
            } else {
                return Err(format!("Error opening file: no such file or directory, path:{loc_path}"));
            }
        }
        None => {
            hdc::info!("task_queue is empty, not need install");
            return Err("Not any installation package was found".to_owned());
        }
    }
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
        task.transfer.transfer_config.path = task.transfer.remote_path.clone();
        Ok(())
    } else {
        hdc::error!("file_manager.open {error_msg}");
        Err(error_msg)
    }
}

async fn init_install(session_id: u32, channel_id: u32, command: &String) -> Result<(), String> {
    let (argv, argc) = Base::split_command_to_args(command);
    if argc < 1 {
        hdc::error!("argc {argc}, {command}");
        return Err("Invalid parameter".to_owned());
    }

    let Some(arc_task) = HostAppTaskMap::get(session_id, channel_id).await else {
        hdc::error!("Get host app task failed");
        return Err("Internal error, Pls try again".to_owned());
    };
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
                task.transfer.task_queue.push(path);
            } else {
                let pathbuff = PathBuf::from(path.clone());
                if pathbuff.is_dir() {
                    task.transfer.task_queue.push(path);
                }
            }
        }
        i += 1;
    }

    if task.transfer.task_queue.is_empty() {
        return Err("Not any installation package was found".to_owned());
    }

    task.transfer.transfer_config.options = options.clone();
    task.transfer.transfer_config.function_name = TRANSFER_FUNC_NAME.to_string();
    task.transfer.is_master = true;
    drop(task);
    install_single(session_id, channel_id).await
}

async fn task_app_install(session_id: u32, channel_id: u32, payload: &[u8]) -> Result<(), String> {
    match String::from_utf8(payload.to_vec()) {
        Ok(str) => {
            hdc::info!("cmd : {str}");
            init_install(session_id, channel_id, &str).await?;
            hdcfile::wake_up_slaver(session_id, channel_id).await;
            put_app_check(session_id, channel_id).await
        }
        Err(e) => {
            hdc::error!("error {}", e);
            let err_msg = "Internal error, Pls try again".to_owned();
            return Err(err_msg);
        }
    }
    Ok(())
}

async fn task_app_uninstall(session_id: u32, channel_id: u32, payload: &[u8]) -> Result<(), String> {
    match String::from_utf8(payload.to_vec()) {
        Ok(str) => {
            hdc::info!("cmd {str}");
            let (argv, argc) = Base::split_command_to_args(&str);
            if argc < 1 {
                hdc::error!("argc {argc}");
                let err_msg = String::from("Invalid input parameters");
                return Err(err_msg);
            }
            let (_opt, pack): (Vec<&String>, Vec<&String>) = argv.iter().partition(|arg| arg.starts_with('-'));
            if pack.len() <= 1 {
                let err_msg = String::from("Invalid input parameters");
                return Err(err_msg);
            }
            let options = argv[1..].join(" ");
            let payload = options.as_bytes().to_vec();
            do_app_uninstall(session_id, channel_id, payload).await;
        }
        Err(e) => {
            println!("error {}", e);
            let err_msg = "Internal error, Pls try again".to_owned();
            return Err(err_msg);
        }
    }
    Ok(())
}

pub async fn command_dispatch(
    session_id: u32, channel_id: u32, command: HdcCommand, payload: &[u8],
) -> Result<bool, &str> {
    match command {
        HdcCommand::AppInit => {
            if let Err(err_msg) = task_app_install(session_id, channel_id, payload).await {
                let _ = transfer::send_channel_msg(channel_id, EchoLevel::FAIL, err_msg).await;
                transfer::TcpMap::end(channel_id).await;
                task_finish(session_id, channel_id).await;
                return Ok(false);
            }
        }
        HdcCommand::AppBegin => {
            let Some(arc_task) = HostAppTaskMap::get(session_id, channel_id).await else {
                hdc::error!("Get host app task failed");
                let err_msg = "Internal error, Pls try again".to_owned();
                let _ = transfer::send_channel_msg(channel_id, EchoLevel::FAIL, err_msg).await;
                transfer::TcpMap::end(channel_id).await;
                task_finish(session_id, channel_id).await;
                return Ok(false);
            };
            let task = arc_task.lock().await;
            hdc::info!("recv HdcCommand::AppBegin");
            hdctransfer::transfer_begin(&task.transfer, HdcCommand::AppData).await;
        }
        HdcCommand::AppUninstall => {
            if let Err(err_msg) = task_app_uninstall(session_id, channel_id, payload).await {
                let _ = transfer::send_channel_msg(channel_id, EchoLevel::FAIL, err_msg).await;
                transfer::TcpMap::end(channel_id).await;
                task_finish(session_id, channel_id).await;
                return Ok(false);
            }
        },
        HdcCommand::AppFinish => {
            hdc::info!("recv HdcCommand::AppFinish");
            do_app_finish(session_id, channel_id, payload).await;
        }
        _ => {
            println!("other command");
        }
    }
    Ok(true)
}
