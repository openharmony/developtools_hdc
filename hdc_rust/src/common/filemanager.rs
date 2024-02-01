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

use std::fs::OpenOptions;
use std::fs::{self, File};

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
                Err(_e) => {
                    println!("failed to open file {:?}", _e);
                    err_msg = format!("Transfer {} failed: {:#?}.", path, _e);
                    result = false;
                }
            }
        }
        (result, err_msg)
    }

    pub fn file_size(&self) -> u64 {
        if let Some(f) = &self.file {
            return f.metadata().unwrap().len();
        }
        0
    }
}
