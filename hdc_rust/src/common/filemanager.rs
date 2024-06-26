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
//! filemanager
#![allow(missing_docs)]

use crate::config::KERNEL_FILE_NODE_SIZE;
#[cfg(not(feature = "host"))]
use crate::utils::hdc_log::*;
use std::fs::OpenOptions;
use std::fs::{self, File};
use std::io::Read;

pub struct FileManager {
    path: Option<String>,
    file: Option<File>,
}

impl FileManager {
    pub fn remove_file(path: &str) -> std::io::Result<()> {
        fs::remove_file(path)
    }

    pub fn new(file_path: String) -> FileManager {
        FileManager {
            path: Some(file_path),
            file: None,
        }
    }

    pub fn open(&mut self) -> (bool, String) {
        let mut result = false;
        let mut err_msg = String::from("");
        if let Some(path) = &self.path {
            let mut _file = OpenOptions::new().read(true).open(path);
            match _file {
                Ok(f) => {
                    self.file = Some(f);
                    result = true;
                }
                Err(e) => {
                    err_msg = format!("Transfer {} failed: {:?}.", path, e);
                    crate::error!("{err_msg}");
                    result = false;
                }
            }
        }
        (result, err_msg)
    }

    pub fn file_size(&self) -> u64 {
        if let Some(f) = &self.file {
            let meta_size: u64 = match f.metadata() {
                Ok(meta) => meta.len(),
                Err(e) => {
                    crate::warn!("failed to get file metadata, error: {:#?}", e);
                    0
                }
            };
            if meta_size == KERNEL_FILE_NODE_SIZE.into() || meta_size == 0 {
                let node_size = self.buffer_read(meta_size) as u64;
                return node_size;
            } else {
                return meta_size;
            }
        }
        0
    }

    pub fn buffer_read(&self, meta_size: u64) -> usize {
        let mut buf = [0u8; KERNEL_FILE_NODE_SIZE as usize];
        let mut read_len = 0usize;
        if let Some(path) = &self.path {
            let mut _file = File::open(path);
            if let Ok(mut f) = _file {
                loop {
                    let single_len = match f.read(&mut buf[read_len..]) {
                        Ok(len) => len,
                        Err(e) => {
                            crate::warn!(
                                "failed to read kernel file node with buffer, error: {:#?}",
                                e
                            );
                            break;
                        }
                    };
                    read_len += single_len;
                    if single_len == 0 || meta_size == 0 {
                        break;
                    }
                }
            }
        }
        read_len
    }
}
