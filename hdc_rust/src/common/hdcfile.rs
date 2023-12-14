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
//! hdcfile
#![allow(missing_docs)]

use crate::transfer;
use std::fs::metadata;

use std::collections::HashMap;
use std::sync::Arc;
use ylong_runtime::sync::Mutex;

use crate::common::filemanager::FileManager;
use crate::common::hdctransfer::*;
use crate::config::CompressType;
use crate::config::HdcCommand;
use crate::config::TaskMessage;
use crate::config::MAX_SIZE_IOBUF;
use crate::serializer::serialize::Serialization;

use super::base::Base;
use super::hdctransfer;
use crate::serializer::native_struct::TransferConfig;
use crate::utils;
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct HdcFile {
    pub file_cnt: u32,
    pub dir_size: u64,
    pub file_size: u64,
    pub file_begin_time: u64,
    pub dir_begin_time: u64,
    pub transfer: HdcTransferBase,
}

impl HdcFile {
    pub fn new(_session_id: u32, _channel_id: u32) -> Self {
        Self {
            transfer: HdcTransferBase::new(_session_id, _channel_id),
            ..Default::default()
        }
    }
}
type HdcFile_ = Arc<Mutex<HdcFile>>;
type FileTaskMap_ = Arc<Mutex<HashMap<(u32, u32), HdcFile_>>>;
pub struct FileTaskMap {}
impl FileTaskMap {
    fn get_instance() -> FileTaskMap_ {
        static mut MAP: Option<FileTaskMap_> = None;
        unsafe {
            MAP.get_or_insert_with(|| Arc::new(Mutex::new(HashMap::new())))
                .clone()
        }
    }

    pub async fn put(session_id: u32, channel_id: u32, value: HdcFile) {
        let map = Self::get_instance();
        let mut map = map.lock().await;
        map.insert((session_id, channel_id), Arc::new(Mutex::new(value)));
    }

    pub async fn exsit(session_id: u32, channel_id: u32) -> bool {
        let arc = Self::get_instance();
        let map = arc.lock().await;
        let task = map.get(&(session_id, channel_id));
        task.is_some()
    }

    pub async fn remove(session_id: u32, channel_id: u32) -> Option<HdcFile_> {
        let arc = Self::get_instance();
        let mut map = arc.lock().await;
        map.remove(&(session_id, channel_id))
    }

    pub async fn get(session_id: u32, channel_id: u32) -> HdcFile_ {
        let arc = Self::get_instance();
        let map = arc.lock().await;
        let task = map.get(&(session_id, channel_id)).unwrap();
        task.clone()
    }
}

async fn check_local_path(session_id: u32, channel_id: u32) -> bool {
    let task = FileTaskMap::get(session_id, channel_id).await;
    let mut file_task = task.lock().await;
    let local_path = file_task.transfer.local_path.clone();
    let mut file_manager = FileManager::new(local_path);
    let (open_result, err_msg) = file_manager.open();
    if open_result {
        file_task.transfer.transfer_config.file_size = file_manager.file_size();
        file_task.transfer.file_size = file_task.transfer.transfer_config.file_size;
        file_task.file_size = file_task.transfer.transfer_config.file_size;
        file_task.transfer.transfer_config.optional_name = file_task.transfer.local_name.clone();
        if transfer::base::CheckCompressVersion::get().await
            && (file_task.transfer.transfer_config.file_size > (MAX_SIZE_IOBUF as u64))
        {
            file_task.transfer.transfer_config.compress_type = CompressType::Lz4 as u8;
        }
        if file_task.transfer.transfer_config.hold_timestamp {}
        file_task.transfer.transfer_config.path = file_task.transfer.remote_path.clone();
        return true;
    } else {
        hdctransfer::echo_client(session_id, channel_id, err_msg.as_bytes().to_vec()).await;
    }
    false
}

async fn echo_finish(session_id: u32, channel_id: u32, msg: String) {
    hdctransfer::echo_client(session_id, channel_id, msg.as_bytes().to_vec()).await;
    task_finish(session_id, channel_id).await;
}

pub async fn begin_transfer(session_id: u32, channel_id: u32, command: &String) -> bool {
    let (argv, argc) = Base::split_command_to_args(command);
    if argc < 2 || !set_master_parameters(session_id, channel_id, command, argc, argv).await {
        echo_finish(
            session_id,
            channel_id,
            "Transfer fail, arguments is invalid.".to_string(),
        )
        .await;
        return false;
    }

    let task = FileTaskMap::get(session_id, channel_id).await;
    let mut task = task.lock().await;
    task.transfer.is_master = true;
    drop(task);

    let ret = check_local_path(session_id, channel_id).await;
    if !ret {
        do_file_finish(session_id, channel_id, &[1]).await;
        return true;
    }

    put_file_check(session_id, channel_id).await;
    true
}

async fn set_master_parameters(
    session_id: u32,
    channel_id: u32,
    _command: &str,
    argc: u32,
    argv: Vec<String>,
) -> bool {
    let task = FileTaskMap::get(session_id, channel_id).await;
    let mut task = task.lock().await;
    let mut i: usize = 0;
    let mut src_argv_index = 0u32;
    while i < argc as usize {
        match &argv[i] as &str {
            "-z" => {
                task.transfer.transfer_config.compress_type = CompressType::Lz4 as u8;
                src_argv_index += 1;
            }
            "-a" => {
                task.transfer.transfer_config.hold_timestamp = true;
                src_argv_index += 1;
            }
            "-sync" => {
                task.transfer.transfer_config.update_if_new = true;
                src_argv_index += 1;
            }
            "-m" => {
                task.transfer.is_file_mode_sync = true;
                src_argv_index += 1;
            }
            "-remote" => {
                src_argv_index += 1;
            }
            "-cwd" => {
                src_argv_index += 2;
                task.transfer.transfer_config.client_cwd = argv.get(i + 1).unwrap().clone();
            }
            _ => {}
        }
        i += 1;
    }
    if argc == src_argv_index {
        return false;
    }
    task.transfer.remote_path = argv.last().unwrap().clone();
    task.transfer.local_path = argv.get(argv.len() - 2).unwrap().clone();
    if task.transfer.server_or_daemon {
        if src_argv_index + 1 == argc {
            return false;
        }
        let cwd = task.transfer.transfer_config.client_cwd.clone();
        task.transfer.local_path = Base::extract_relative_path(&cwd, &task.transfer.local_path);
    } else if src_argv_index + 1 == argc {
        task.transfer.remote_path = String::from(".");
        task.transfer.local_path = argv.get((argc - 1) as usize).unwrap().clone();
    }
    task.transfer.local_name = Base::get_file_name(&mut task.transfer.local_path).unwrap();
    let file = metadata(task.transfer.local_path.clone());
    if let Ok(f) = file {
        if !f.is_dir() {
            task.transfer.is_dir = false;
            return true;
        }
        task.transfer.is_dir = true;
        task.transfer.task_queue = get_sub_files_resurively(&task.transfer.local_path.clone());
        task.transfer.base_local_path = task.transfer.local_path.clone();

        if !task.transfer.task_queue.is_empty() {
            task.transfer.local_path = task.transfer.task_queue.pop().unwrap();
            task.transfer.local_name =
                task.transfer.local_path[task.transfer.base_local_path.len()..].to_string();
        } else {
            return false;
        }
    } else if file.is_err() {
        return false;
    }
    true
}

async fn put_file_check(session_id: u32, channel_id: u32) {
    let task = FileTaskMap::get(session_id, channel_id).await;
    let task = task.lock().await;
    let file_check_message = TaskMessage {
        channel_id,
        command: HdcCommand::FileCheck,
        payload: task.transfer.transfer_config.serialize(),
    };
    transfer::put(task.transfer.session_id, file_check_message).await;
}

pub async fn check_slaver(session_id: u32, channel_id: u32, _payload: &[u8]) -> bool {
    let task = FileTaskMap::get(session_id, channel_id).await;
    let task = &mut task.lock().await;
    let mut transconfig = TransferConfig {
        ..Default::default()
    };
    let _ = transconfig.parse(_payload.to_owned());
    task.transfer.file_size = transconfig.file_size;
    task.file_size = transconfig.file_size;
    task.transfer.local_path = transconfig.path;
    task.transfer.is_master = false;
    task.transfer.index = 0;
    let mut error = String::new();
    let local_path = task.transfer.local_path.clone();
    let optional_name = transconfig.optional_name.clone();
    task.transfer.transfer_config.compress_type = transconfig.compress_type;
    let check_result =
        hdctransfer::check_local_path(&mut task.transfer, &local_path, &optional_name, &mut error);
    if !check_result {
        return false;
    }
    if task.transfer.transfer_config.update_if_new {
        return false;
    }
    if task.dir_begin_time == 0 {
        task.dir_begin_time = utils::get_current_time();
    }
    task.file_begin_time = utils::get_current_time();
    true
}

async fn put_file_begin(session_id: u32, channel_id: u32) {
    let file_begin_message = TaskMessage {
        channel_id,
        command: HdcCommand::FileBegin,
        payload: Vec::<u8>::new(),
    };
    transfer::put(session_id, file_begin_message).await;
}

async fn transfer_next(session_id: u32, channel_id: u32) -> bool {
    let task = FileTaskMap::get(session_id, channel_id).await;
    let mut task = task.lock().await;
    task.transfer.local_path = task.transfer.task_queue.pop().unwrap();
    task.transfer.local_name =
        task.transfer.local_path[task.transfer.base_local_path.len()..].to_string();
    drop(task);
    check_local_path(session_id, channel_id).await
}

async fn on_all_transfer_finish(session_id: u32, channel_id: u32) {
    let task = FileTaskMap::get(session_id, channel_id).await;
    let task = task.lock().await;
    let size = if task.file_cnt > 1 {
        task.dir_size
    } else {
        task.file_size
    };
    let time = if task.file_cnt > 1 {
        utils::get_current_time() - task.dir_begin_time
    } else {
        utils::get_current_time() - task.file_begin_time
    };
    let rate = size as f64 / time as f64;
    let message = format!(
        "FileTransfer finish, Size:{}, File count = {}, time:{}ms rate:{:.2}kB/s",
        size, task.file_cnt, time, rate
    );
    hdctransfer::echo_client(
        task.transfer.session_id,
        task.transfer.channel_id,
        message.as_bytes().to_vec(),
    )
    .await;

    hdctransfer::close_channel(channel_id).await;
}

async fn do_file_finish(session_id: u32, channel_id: u32, _payload: &[u8]) {
    if _payload[0] == 1 {
        let task = FileTaskMap::get(session_id, channel_id).await;
        let task = task.lock().await;
        let empty = task.transfer.task_queue.is_empty();
        drop(task);
        if !empty && transfer_next(session_id, channel_id).await {
            put_file_check(session_id, channel_id).await;
        } else {
            let _finish_message = TaskMessage {
                channel_id,
                command: HdcCommand::FileFinish,
                payload: [0].to_vec(),
            };

            transfer::put(session_id, _finish_message).await;
        }
    } else {
        on_all_transfer_finish(session_id, channel_id).await;
        task_finish(session_id, channel_id).await;
    }
}

async fn put_file_finish(session_id: u32, channel_id: u32) {
    let task = FileTaskMap::get(session_id, channel_id).await;
    let mut task = task.lock().await;
    let _payload: [u8; 1] = [1];
    task.file_cnt += 1;
    task.dir_size += task.file_size;
    let task_finish_message = TaskMessage {
        channel_id,
        command: HdcCommand::FileFinish,
        payload: _payload.to_vec(),
    };
    transfer::put(session_id, task_finish_message).await;
}

pub async fn command_dispatch(
    session_id: u32,
    channel_id: u32,
    _command: HdcCommand,
    _payload: &[u8],
    _payload_size: u16,
) -> bool {
    match _command {
        HdcCommand::FileInit => {
            let s = String::from_utf8(_payload.to_vec());
            match s {
                Ok(str) => {
                    begin_transfer(session_id, channel_id, &str).await;
                }
                Err(e) => {
                    println!("error {}", e);
                    let err_msg = format!("Transfer failed: arguments is invalid {:#?}", e);
                    echo_finish(session_id, channel_id, err_msg.to_string()).await;
                }
            }
        }
        HdcCommand::FileCheck => {
            if !check_slaver(session_id, channel_id, _payload).await {
                hdctransfer::echo_client(
                    session_id,
                    channel_id,
                    "Transfer failed: create dir fail".as_bytes().to_vec(),
                )
                .await;
                task_finish(session_id, channel_id).await;
            } else {
                put_file_begin(session_id, channel_id).await;
            }
        }
        HdcCommand::FileBegin => {
            let task = FileTaskMap::get(session_id, channel_id).await;
            let task = task.lock().await;
            hdctransfer::transfer_begin(&task.transfer, HdcCommand::FileData).await;
        }
        HdcCommand::FileData => {
            let task = FileTaskMap::get(session_id, channel_id).await;
            let mut task = task.lock().await;
            if hdctransfer::transfer_data(&mut task.transfer, _payload) {
                drop(task);
                put_file_finish(session_id, channel_id).await;
            } else {
            }
        }
        HdcCommand::FileMode => {}
        HdcCommand::FileFinish => {
            do_file_finish(session_id, channel_id, _payload).await;
        }
        _ => {
            println!("others");
        }
    }

    true
}

async fn task_finish(session_id: u32, channel_id: u32) {
    hdctransfer::transfer_task_finish(channel_id, session_id).await;
}
