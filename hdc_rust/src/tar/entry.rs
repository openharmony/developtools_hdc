/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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
//! entry

use std::{
    fs,
    io::{self, Write},
    path::{Path, PathBuf},
};

use crate::tar::header::{self, Header, TypeFlage, HEADER_LEN};
#[allow(unused)]
use crate::utils::hdc_log::*;

/// Entry
pub struct Entry {
    /// 每个文件的头
    header: Header,
    /// 文件大小 (即data数组中不包含填充字段的长度)
    need_size: u64,
    /// 路径前缀，不为空时，保存到header时去掉前缀，读取文件时进行拼接
    // prefix: String,
    prefix: PathBuf,
    /// 文件数据
    data: Vec<u8>,
}

impl Entry {
    /// new entry form file
    pub fn new(prefix: PathBuf, path: &str) -> Self {
        let mut entry = Self {
            header: Header::new(),
            need_size: 0,
            prefix,
            data: Vec::new(),
        };
        match fs::metadata(path) {
            Ok(metadata) => {
                let file_size = metadata.len();
                if metadata.is_file() {
                    entry.header.updata_size(file_size as usize);
                    entry.need_size = file_size;
                    entry.header.updata_file_type(TypeFlage::OrdinaryFile);
                } else if metadata.is_dir() {
                    entry.header.updata_size(0);
                    entry.header.updata_file_type(TypeFlage::Directory);
                }
            }
            Err(_) => return entry,
        };

        if let Err(e) = entry.updata_name(path.to_string()) {
            crate::error!("{e}");
        }

        entry
    }

    /// new entry from data
    pub fn create_from_raw_data(data: &[u8; 512]) -> Result<Self, std::io::Error> {
        let header = Header::create_from_raw_data(data);
        let need_size = header.size();
        if header.is_invalid() {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "header is invalid",
            ));
        }
        Ok(Self {
            header,
            need_size,
            prefix: PathBuf::new(),
            data: Vec::new(),
        })
    }

    /// Get name
    pub fn name(&self) -> String {
        let name = self.prefix.join(self.header.name());
        name.display().to_string()
    }

    /// updata name
    pub fn updata_name(&mut self, name: String) -> Result<(), &str> {
        if self.prefix.components().count() != 0 {
            let name_path = Path::new(&name);
            if let Ok(sort_path) = name_path.strip_prefix(self.prefix.clone()) {
                return self.header.updata_name(sort_path.display().to_string());
            }
        }
        self.header.updata_name(name)
    }

    /// The data read is of the expected size
    pub fn is_finish(&self) -> bool {
        self.need_size == 0
    }

    /// Type is invalid
    pub fn is_invalid(&self) -> bool {
        self.header.is_invalid()
    }

    /// Add data to entry
    pub fn add_data(&mut self, data: &[u8]) {
        if self.need_size == 0 {
            return;
        }
        if self.need_size > data.len() as u64 {
            self.data.extend_from_slice(data);
            self.need_size -= data.len() as u64;
        } else {
            self.data
                .extend_from_slice(&data[..self.need_size as usize]);
            self.need_size = 0;
        }
    }

    /// Get file size
    pub fn size(&self) -> u64 {
        self.header.size()
    }

    /// Write entry to file
    pub fn write_to_file(&self, path: &Path) -> Result<(), std::io::Error> {
        if !self.is_finish() {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                format!("{} data is not load finish", self.name()),
            ));
        }

        match self.header.file_type() {
            header::TypeFlage::OrdinaryFile => {
                let file_path = path.join(self.name());
                let mut f = fs::OpenOptions::new()
                    .read(true)
                    .write(true)
                    .create(true)
                    .open(file_path)?;
                f.write_all(&self.data)?;
            }
            header::TypeFlage::Directory => {
                let dir_path = path.join(self.name());
                fs::create_dir_all(dir_path)?;
            }
            file_type => {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    format!("{:?} not yet support. {}", file_type, self.name()),
                ));
            }
        }

        Ok(())
    }

    /// Read the file content from the file pointed to by name() to _file
    pub fn read_data_to_file(&mut self, _file: &mut fs::File) -> Result<(), std::io::Error> {
        match self.header.file_type() {
            TypeFlage::OrdinaryFile => {
                let mut buff = [0u8; 512];
                self.header.get_bytes(&mut buff);
                _file.write_all(&buff)?;
                let mut in_file = fs::File::open(self.name())?;
                io::copy(&mut in_file, _file)?;
                let pading = HEADER_LEN - (self.need_size % HEADER_LEN);
                if pading < HEADER_LEN {
                    let empty_buff = [0u8; 512];
                    _file.write_all(&empty_buff[..pading as usize])?;
                }
            }
            TypeFlage::Directory => {
                let mut buff = [0u8; 512];
                self.header.get_bytes(&mut buff);
                _file.write_all(&buff)?;
            }
            file_type => {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    format!("file type error, {:?}, path: {}", file_type, self.name()),
                ))
            }
        };
        Ok(())
    }
}
