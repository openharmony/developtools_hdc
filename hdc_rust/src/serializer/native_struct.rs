/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
//! session_struct
#![allow(missing_docs)]

#[derive(Debug, Default, PartialEq, Eq, Clone)]
pub struct SessionHandShake {
    pub banner: String,
    pub auth_type: u8,
    pub session_id: u32,
    pub connect_key: String,
    pub buf: String,
    pub version: String,
}

#[derive(Debug, Default, PartialEq, Eq)]
pub struct PayloadProtect {
    pub channel_id: u32,
    pub command_flag: u32,
    pub check_sum: u8,
    pub v_code: u8,
}

#[derive(Debug, Default, PartialEq, Eq)]
pub struct PayloadHead {
    pub flag: [u8; 2],
    pub reserve: [u8; 2],
    pub protocol_ver: u8,
    pub head_size: u16,
    pub data_size: u32,
}

#[allow(unused)]
#[derive(Debug, Default, PartialEq, Eq)]
pub struct UartHead {
    pub flag: [u8; 2],
    pub option: u16,
    pub session_id: u32,
    pub data_size: u32,
    pub package_index: u32,
    pub data_checksum: u32,
    pub head_checksum: u32,
}

#[allow(unused)]
#[derive(Debug, Default, PartialEq, Eq)]
pub struct UsbHead {
    pub flag: [u8; 2],
    pub option: u8,
    pub session_id: u32,
    pub data_size: u32,
}

#[derive(Debug, Default, PartialEq, Eq)]
pub struct TransferConfig {
    pub file_size: u64,
    pub atime: u64,
    pub mtime: u64,
    pub options: String,
    pub path: String,
    pub optional_name: String,
    pub update_if_new: bool,
    pub compress_type: u8,
    pub hold_timestamp: bool,
    pub function_name: String,
    pub client_cwd: String,
    pub reserve1: String,
    pub reserve2: String,
}

#[derive(Debug, Default, PartialEq, Eq)]
pub struct FileMode {
    pub perm: u64,
    pub u_id: u64,
    pub g_id: u64,
    pub context: String,
    pub full_name: String,
}

#[derive(Debug, Default, PartialEq, Eq)]
pub struct TransferPayload {
    pub index: u64,
    pub compress_type: u8,
    pub compress_size: u32,
    pub uncompress_size: u32,
}
