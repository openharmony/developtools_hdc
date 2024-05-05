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
//! app
#![allow(missing_docs)]

use hdc::common::filemanager::FileManager;
use hdc::common::hdctransfer::{self, HdcTransferBase};
use hdc::config;
use hdc::config::HdcCommand;
use hdc::config::TaskMessage;
use hdc::serializer::native_struct::TransferConfig;
use hdc::serializer::serialize::Serialization;
use hdc::transfer;
use std::collections::HashMap;
use std::sync::Arc;
use ylong_runtime::sync::Mutex;

use std::fs::{create_dir_all, File, *};
use std::io::{self, Error, ErrorKind};
use std::process::Command;

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct DaemonAppTask {
    pub transfer: HdcTransferBase,
}

impl DaemonAppTask {
    pub fn new(_session_id: u32, _channel_id: u32) -> Self {
        Self {
            transfer: HdcTransferBase::new(_session_id, _channel_id),
        }
    }
}

type DaemonAppTask_ = Arc<Mutex<DaemonAppTask>>;
type AppTaskMap_ = Arc<Mutex<HashMap<(u32, u32), DaemonAppTask_>>>;
pub struct AppTaskMap {}
impl AppTaskMap {
    fn get_instance() -> AppTaskMap_ {
        static mut MAP: Option<AppTaskMap_> = None;
        unsafe {
            MAP.get_or_insert_with(|| Arc::new(Mutex::new(HashMap::new())))
                .clone()
        }
    }

    pub async fn put(session_id: u32, channel_id: u32, value: DaemonAppTask) {
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

    pub async fn remove(session_id: u32, channel_id: u32) -> Option<DaemonAppTask_> {
        let arc = Self::get_instance();
        let mut map = arc.lock().await;
        map.remove(&(session_id, channel_id))
    }

    pub async fn get(session_id: u32, channel_id: u32) -> DaemonAppTask_ {
        let arc = Self::get_instance();
        let map = arc.lock().await;
        let task = map.get(&(session_id, channel_id)).unwrap();
        task.clone()
    }

    async fn stop_task(session_id: u32) {
        let arc = Self::get_instance();
        let map = arc.lock().await;
        for _iter in map.iter() {
            if _iter.0.0 != session_id {
                continue;
            }
            hdctransfer::transfer_task_finish(_iter.0.1, session_id).await;
            let mut task = _iter.1.lock().await;
            task.transfer.stop_run = true;
        }
    }

    async fn dump_task() -> String {
        let arc = Self::get_instance();
        let map = arc.lock().await;
        let mut result = String::new();
        for _iter in map.iter() {
            let task = _iter.1.lock().await;
            let command = task.transfer.command_str.clone();
            let line = format!("session_id:{},\tchannel_id:{},\tcommand:{}",
                _iter.0.0, _iter.0.1, command);
            result.push_str(line.as_str());
        }
        result
    }
}

async fn do_app_check(session_id: u32, channel_id: u32, _payload: &[u8]) -> bool {
    let arc = AppTaskMap::get(session_id, channel_id).await;
    let mut task = arc.lock().await;
    let mut transconfig = TransferConfig {
        ..Default::default()
    };
    let _ = transconfig.parse(_payload.to_owned());
    task.transfer.transfer_config.options = transconfig.options.clone();
    task.transfer.transfer_config.function_name = transconfig.function_name.clone();
    let tmp_dir = String::from(config::INSTALL_TMP_DIR);
    let local_path = tmp_dir.clone() + transconfig.optional_name.as_str();
    task.transfer.command_str = format!("[{}],\tlocal_path:{}\n",
        transconfig.function_name, local_path);
    task.transfer.is_master = false;
    task.transfer.local_path = local_path;
    task.transfer.file_size = transconfig.file_size;
    task.transfer.index = 0;
    let state = metadata(tmp_dir.clone());
    if let Ok(metadata_obj) = state {
        if metadata_obj.is_dir() {
            return File::create(task.transfer.local_path.clone()).is_ok();
        }
    } else {
        let _ = create_dir_all(tmp_dir);
        return File::create(task.transfer.local_path.clone()).is_ok();
    }
    false
}

async fn put_file_begin(session_id: u32, channel_id: u32) {
    let file_begin_message = TaskMessage {
        channel_id,
        command: HdcCommand::AppBegin,
        payload: Vec::<u8>::new(),
    };
    transfer::put(session_id, file_begin_message).await;
}

async fn put_app_finish(
    session_id: u32,
    channel_id: u32,
    mode: u8,
    exit_status: u8,
    result: &mut [u8],
) {
    let mut msg = Vec::<u8>::new();
    msg.push(mode);
    msg.push(exit_status);
    msg.append(&mut result.to_vec());

    let app_finish_message = TaskMessage {
        channel_id,
        command: HdcCommand::AppFinish,
        payload: msg.clone(),
    };
    transfer::put(session_id, app_finish_message).await;
}

async fn app_uninstall(session_id: u32, channel_id: u32, _payload: &[u8]) {
    let mut str = String::from_utf8(_payload.to_vec()).unwrap();
    str = str.trim_end_matches('\0').to_string();

    let (opt, package) : (Vec<String>, Vec<String>) = str.split(' ')
        .map(String::from)
        .partition(|word| word.starts_with('-'));
    do_app_uninstall(session_id, channel_id, opt.join(" "), package.join(" ")).await;
}

async fn handle_execute_result(
    session_id: u32,
    channel_id: u32,
    result: Result<Vec<u8>, Error>,
    mode: u8,
) {
    match &result {
        Ok(message) => {
            let mut m: Vec<u8> = message.clone();
            put_app_finish(session_id, channel_id, mode, 1, &mut m[..]).await;
        }
        Err(err) => {
            put_app_finish(
                session_id,
                channel_id,
                mode,
                0,
                &mut err.to_string().into_bytes()[..],
            )
            .await;
        }
    }
}

async fn do_app_uninstall(session_id: u32, channel_id: u32, options: String, package: String) {
    let mode = config::AppModeType::UnInstall as u8;
    if !options.contains('n') {
        let result = execute_cmd(&format!("bm uninstall {} -n {}", options, package));
        handle_execute_result(session_id, channel_id, result, mode).await;
    } else {
        let result = execute_cmd(&format!("bm uninstall {} {}", options, package));
        handle_execute_result(session_id, channel_id, result, mode).await;
    }
}

async fn do_app_install(session_id: u32, channel_id: u32) {
    let arc = AppTaskMap::get(session_id, channel_id).await;
    let task = arc.lock().await;
    let options = task.transfer.transfer_config.options.clone();
    let local_path = task.transfer.local_path.clone();
    drop(task);
    let mode = config::AppModeType::Install as u8;
    if !options.contains('p') && !options.contains('s') {
        let result = execute_cmd(&format!("bm install {} -p {}", options, local_path));
        handle_execute_result(session_id, channel_id, result, mode).await;
    } else {
        let result = execute_cmd(&format!("bm install {} {}", options, local_path));
        handle_execute_result(session_id, channel_id, result, mode).await;
    }
    let _ = FileManager::remove_file(local_path.as_str());
}

fn execute_cmd(cmd: &String) -> io::Result<Vec<u8>> {
    let result = Command::new(config::SHELL_PROG).args(["-c", cmd]).output();
    match result {
        Ok(output) => {
            let msg = [output.stdout, output.stderr].concat();
            let mut str = String::from_utf8(msg).unwrap();
            str = str.replace('\n', " ");
            Ok(str.into_bytes())
        }
        Err(e) => Err(Error::new(ErrorKind::Other, e.to_string())),
    }
}

async fn on_transfer_finish(session_id: u32, channel_id: u32) {
    let arc = AppTaskMap::get(session_id, channel_id).await;
    let task = arc.lock().await;
    let function_name = task.transfer.transfer_config.function_name.clone();
    drop(task);
    if function_name == config::TRANSFER_FUNC_NAME {
        do_app_install(session_id, channel_id).await;
    }
}

async fn transfer_fail(session_id: u32, channel_id: u32, error: &str) {
    let mode = config::AppModeType::Install as u8;
    put_app_finish(
        session_id,
        channel_id,
        mode,
        0,
        &mut error.to_string().into_bytes()[..],
    )
    .await;
}

pub async fn command_dispatch(
    session_id: u32,
    channel_id: u32,
    _command: HdcCommand,
    _payload: &[u8],
    _payload_size: u16,
) -> bool {
    println!("app commmand:{:#?}", _command);
    match _command {
        HdcCommand::AppCheck => {
            if do_app_check(session_id, channel_id, _payload).await {
                put_file_begin(session_id, channel_id).await;
            } else {
                transfer_fail(session_id, channel_id, "check file fail.").await;
            }
        }
        HdcCommand::AppUninstall => {
            app_uninstall(session_id, channel_id, _payload).await;
        }
        HdcCommand::AppData => {
            let arc = AppTaskMap::get(session_id, channel_id).await;
            let mut task = arc.lock().await;
            if task.transfer.stop_run {
                return false;
            }
            if hdctransfer::transfer_data(&mut task.transfer, _payload) {
                drop(task);
                on_transfer_finish(session_id, channel_id).await;
            }
        }
        _ => {
            println!("other command");
        }
    }
    true
}

pub async fn stop_task(session_id: u32) {
    AppTaskMap::stop_task(session_id).await;
}

pub async fn dump_task() -> String {
    AppTaskMap::dump_task().await
}