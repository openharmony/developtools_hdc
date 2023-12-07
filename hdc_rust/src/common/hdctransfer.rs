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
//! hdctransfer
#![allow(missing_docs)]

use std::collections::VecDeque;

use crate::common::base::Base;
use crate::config::HdcCommand;
use crate::config::TaskMessage;
use crate::config::*;
use crate::serializer::native_struct::TransferConfig;
use crate::serializer::native_struct::TransferPayload;
use crate::serializer::serialize::Serialization;
use crate::transfer;
use std::fs::metadata;
use std::fs::OpenOptions;
use std::fs::{self, create_dir_all, File};
use std::io::{Read, Seek, Write};
use std::path::PathBuf;
use std::sync::Arc;
use ylong_runtime::sync::Mutex;
use ylong_runtime::task::JoinHandle;

extern "C" {
    fn LZ4_compress_transfer(
        data: *const libc::c_char,
        data_compress: *mut libc::c_char,
        data_size: i32,
        compress_capacity: i32,
    ) -> i32;
    fn LZ4_decompress_transfer(
        data: *const libc::c_char,
        data_decompress: *mut libc::c_char,
        data_size: i32,
        decompress_capacity: i32,
    ) -> i32;
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct HdcTransferBase {
    pub need_close_notify: bool,
    pub io_index: u64,
    pub last_error: u32,
    pub is_io_finish: bool,
    pub is_master: bool,
    pub remote_path: String,
    pub base_local_path: String,
    pub local_path: String,
    pub server_or_daemon: bool,
    pub task_queue: Vec<String>,
    pub local_name: String,
    pub is_dir: bool,
    pub file_size: u64,
    pub dir_size: u64,
    pub session_id: u32,
    pub channel_id: u32,
    pub index: u64,
    pub file_cnt: u32,
    pub is_file_mode_sync: bool,
    pub file_begin_time: u64,
    pub dir_begin_time: u64,

    pub transfer_config: TransferConfig,
}

impl HdcTransferBase {
    pub fn new(_session_id: u32, _channel_id: u32) -> Self {
        Self {
            need_close_notify: false,
            io_index: 0,
            last_error: 0,
            is_io_finish: false,
            is_master: false,
            remote_path: String::new(),
            base_local_path: String::new(),
            local_path: String::new(),
            server_or_daemon: false,
            task_queue: Vec::<String>::new(),
            local_name: String::new(),
            is_dir: false,
            file_size: 0,
            dir_size: 0,
            session_id: _session_id,
            channel_id: _channel_id,
            index: 0,
            file_cnt: 0,
            is_file_mode_sync: false,
            file_begin_time: 0,
            dir_begin_time: 0,
            transfer_config: TransferConfig::default(),
        }
    }
}
pub fn check_local_path(
    transfer: &mut HdcTransferBase,
    _local_path: &str,
    _optional_name: &str,
    _error: &mut str,
) -> bool {
    let file = metadata(_local_path);
    if let Ok(f) = file {
        transfer.is_dir = f.is_dir();
        if f.is_dir() && !transfer.local_path.ends_with(Base::get_path_sep()) {
            transfer
                .local_path
                .push_str(Base::get_path_sep().to_string().as_str());
        }
    }
    let mut op = _optional_name.replace('\\', Base::get_path_sep().to_string().as_str());
    op = op.replace('/', Base::get_path_sep().to_string().as_str());

    if op.contains(Base::get_path_sep()) && !transfer.local_path.ends_with(Base::get_path_sep()) {
        transfer
            .local_path
            .push_str(Base::get_path_sep().to_string().as_str());
    }

    if transfer.local_path.ends_with(Base::get_path_sep()) {
        transfer.local_path.push_str(op.as_str());
    }
    if transfer.local_path.ends_with(Base::get_path_sep()) {
        create_dir_all(transfer.local_path.clone()).is_ok()
    } else {
        let last = transfer.local_path.rfind(Base::get_path_sep());
        match last {
            Some(index) => {
                let result = create_dir_all(&transfer.local_path[0..index]);
                if result.is_ok() {
                    File::create(transfer.local_path.clone()).is_ok()
                } else {
                    false
                }
            }
            None => File::create(transfer.local_path.clone()).is_ok(),
        }
    }
}

fn spawn_handler(
    _command_data: HdcCommand,
    index: usize,
    local_path: String,
    _channel_id_: u32,
    transfer_config: &TransferConfig,
) -> JoinHandle<(bool, TaskMessage)> {
    let thread_path_ref = Arc::new(Mutex::new(local_path));
    let pos = (index * FILE_PACKAGE_PAYLOAD_SIZE) as u64;
    let compress_type = transfer_config.compress_type;
    ylong_runtime::spawn(async move {
        let path = thread_path_ref.lock().await;
        let mut file = File::open(&*path).unwrap();
        let _ = file.seek(std::io::SeekFrom::Start(pos));
        let mut total = Vec::from([0; FILE_PACKAGE_HEAD]);
        let mut buf: [u8; FILE_PACKAGE_PAYLOAD_SIZE] = [0; FILE_PACKAGE_PAYLOAD_SIZE];
        let mut data_buf: [u8; FILE_PACKAGE_PAYLOAD_SIZE] = [0; FILE_PACKAGE_PAYLOAD_SIZE];
        let read_len = file.read(&mut buf[..]).unwrap();
        let transfer_compress_type = match CompressType::try_from(compress_type) {
            Ok(compress_type) => compress_type,
            Err(_) => CompressType::None,
        };

        let mut header: TransferPayload = TransferPayload {
            index: pos,
            compress_type,
            compress_size: 0,
            uncompress_size: 0,
        };
        header.uncompress_size = read_len as u32;
        let capacity = read_len as i32;

        match transfer_compress_type {
            CompressType::Lz4 => {
                let compress_size: i32;
                header.compress_type = CompressType::Lz4 as u8;
                unsafe {
                    compress_size = LZ4_compress_transfer(
                        buf.as_ptr() as *const libc::c_char,
                        data_buf.as_ptr() as *mut libc::c_char,
                        capacity,
                        capacity,
                    );
                }
                if compress_size > 0 {
                    header.compress_size = compress_size as u32;
                } else {
                    header.compress_type = CompressType::None as u8;
                    header.compress_size = read_len as u32;
                    data_buf = buf;
                }
            }
            _ => {
                header.compress_type = CompressType::None as u8;
                header.compress_size = read_len as u32;
                data_buf = buf;
            }
        }

        let head_buffer = header.serialize();
        total[..head_buffer.len()].copy_from_slice(&head_buffer[..]);
        let data_len = header.compress_size as usize;
        total.append(&mut data_buf[..data_len].to_vec());
        let _data_message = TaskMessage {
            channel_id: _channel_id_,
            command: _command_data,
            payload: total,
        };
        (read_len != FILE_PACKAGE_PAYLOAD_SIZE, _data_message)
    })
}

pub async fn read_and_send_data(
    local_path: &str,
    session_id: u32,
    _channel_id_: u32,
    _file_size: u64,
    _command_data: HdcCommand,
    transfer_config: &TransferConfig,
) -> bool {
    const MAX_WORKER_COUNT: usize = 5;
    let mut pieces_count = (_file_size / FILE_PACKAGE_PAYLOAD_SIZE as u64) as usize;
    if pieces_count == 0 {
        pieces_count = 1;
    }
    let workers_count = if pieces_count > MAX_WORKER_COUNT {
        MAX_WORKER_COUNT
    } else {
        pieces_count
    };
    let mut index = 0;
    let mut queue = VecDeque::new();
    while index < workers_count {
        let worker = spawn_handler(
            _command_data,
            index,
            local_path.to_owned(),
            _channel_id_,
            transfer_config,
        );
        queue.push_back(worker);
        index += 1;
    }
    loop {
        if queue.is_empty() {
            break;
        }
        let handler = queue.pop_front();
        let handler = handler.unwrap();
        let (is_finish, task_message) = handler.await.unwrap();
        transfer::put(session_id, task_message).await;
        if is_finish {
            return false;
        }

        if ((index * FILE_PACKAGE_PAYLOAD_SIZE) as u64) < _file_size {
            let worker = spawn_handler(
                _command_data,
                index,
                local_path.to_owned(),
                _channel_id_,
                transfer_config,
            );
            queue.push_back(worker);
            index += 1;
        }
    }
    true
}

pub fn recv_and_write_file(tbase: &mut HdcTransferBase, _data: &[u8]) -> bool {
    let mut header = TransferPayload {
        ..Default::default()
    };
    let _ = header.parse(_data[..FILE_PACKAGE_HEAD].to_vec());
    let file_index = header.index;
    let mut buffer = _data[FILE_PACKAGE_HEAD..].to_vec();
    let compress_type = match CompressType::try_from(tbase.transfer_config.compress_type) {
        Ok(compress_type) => compress_type,
        Err(_) => CompressType::None,
    };

    if let CompressType::Lz4 = compress_type {
        let buf: [u8; FILE_PACKAGE_PAYLOAD_SIZE] = [0; FILE_PACKAGE_PAYLOAD_SIZE];
        let decompress_size = unsafe {
            LZ4_decompress_transfer(
                _data[FILE_PACKAGE_HEAD..].as_ptr() as *const libc::c_char,
                buf.as_ptr() as *mut libc::c_char,
                header.compress_size as i32,
                header.uncompress_size as i32,
            )
        };
        if decompress_size > 0 {
            buffer = buf[..(decompress_size as usize)].to_vec();
        }
    }

    let path = tbase.local_path.clone();
    let write_buf = buffer.clone();
    ylong_runtime::spawn(async move {
        let mut file = OpenOptions::new()
            .write(true)
            .create(true)
            .open(path)
            .unwrap();
        let _ = file.seek(std::io::SeekFrom::Start(file_index));
        file.write_all(write_buf.as_slice()).unwrap();
    });

    tbase.index += buffer.len() as u64;
    if tbase.index >= tbase.file_size {
        return true;
    }
    false
}

pub fn get_sub_files_resurively(_path: &String) -> Vec<String> {
    let mut result = Vec::new();
    let dir_path = PathBuf::from(_path);
    for entry in fs::read_dir(dir_path).unwrap() {
        let path = entry.unwrap().path();
        if path.is_file() {
            result.push(path.display().to_string());
        } else {
            let p = path.display().to_string();
            let mut sub_list = get_sub_files_resurively(&p);
            result.append(&mut sub_list);
        }
    }
    result.sort();
    result
}

pub async fn transfer_begin(transfer: &HdcTransferBase, _command_data: HdcCommand) {
    let local_path_ = transfer.local_path.clone();

    read_and_send_data(
        &local_path_,
        transfer.session_id,
        transfer.channel_id,
        transfer.file_size,
        _command_data,
        &transfer.transfer_config,
    )
    .await;
}

pub fn transfer_data(tbase: &mut HdcTransferBase, _payload: &[u8]) -> bool {
    recv_and_write_file(tbase, _payload)
}

pub async fn transfer_task_finish(channel_id: u32, _session_id: u32) {
    let task_message = TaskMessage {
        channel_id,
        command: HdcCommand::KernelChannelClose,
        payload: [1].to_vec(),
    };
    transfer::put(_session_id, task_message).await;
}

pub async fn transfer_file_finish(channel_id: u32, _session_id: u32, comamnd_finish: HdcCommand) {
    let task_message = TaskMessage {
        channel_id,
        command: comamnd_finish,
        payload: [1].to_vec(),
    };
    transfer::put(_session_id, task_message).await;
}

pub async fn close_channel(channel_id: u32) {
    transfer::TcpMap::end(channel_id).await;
}

pub async fn echo_client(session_id: u32, channel_id: u32, payload: Vec<u8>) {
    let echo_message = TaskMessage {
        channel_id,
        command: HdcCommand::KernelEchoRaw,
        payload,
    };
    transfer::put(session_id, echo_message).await;
}
