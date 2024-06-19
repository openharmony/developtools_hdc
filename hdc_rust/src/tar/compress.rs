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
//! compress

use std::{
    fs,
    io::{self, Error},
    path::{Path, PathBuf},
};

use crate::tar::entry::Entry;
#[allow(unused)]
use crate::utils::hdc_log::*;

/// Compress tar
#[derive(Default)]
pub struct Compress {
    entrys: Vec<Entry>,
    prefix: PathBuf,
    max_count: usize,
}

impl Compress {
    /// new Compress
    pub fn new() -> Self {
        Self {
            entrys: Vec::new(),
            prefix: PathBuf::new(),
            max_count: 0,
        }
    }

    /// updata prefix
    pub fn updata_prefix(&mut self, prefix: PathBuf) {
        self.prefix = prefix;
    }

    /// updata max conunt
    /// default: 0, no limit
    /// other: Maximum number of entries allowed to be saved
    pub fn updata_max_count(&mut self, count: usize) {
        self.max_count = count;
    }

    fn add_path_recursion(&mut self, path: &Path) -> io::Result<()> {
        if !path.is_dir() {
            self.add_entry(&path.display().to_string())?;
            return Ok(());
        }

        self.add_entry(&path.display().to_string())?;

        for entry in fs::read_dir(path)? {
            let entry = entry?;
            let entry_path = entry.path();
            self.add_path_recursion(&entry_path)?;
        }

        Ok(())
    }

    /// Add a path that needs to be packaged
    pub fn add_path(&mut self, path: &Path) -> io::Result<()> {
        self.add_path_recursion(path)
    }

    fn add_entry(&mut self, file: &str) -> io::Result<()> {
        if self.max_count > 0 && self.entrys.len() > self.max_count {
            return Err(Error::new(
                io::ErrorKind::Other,
                format!(
                    "Exceeded the set maximum value, the maximum value is set to {}",
                    self.max_count
                ),
            ));
        }

        if let Some(prefix) = self.prefix.to_str() {
            if prefix == file {
                crate::debug!("Ignoring compressed root directory");
                return Ok(());
            }
        }

        let entry = Entry::new(self.prefix.clone(), file);
        self.entrys.push(entry);

        Ok(())
    }

    /// 开始将数据写入压缩包
    pub fn compress(&mut self, file_path: PathBuf) -> io::Result<()> {
        if file_path.exists() && file_path.is_dir() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                format!("{} is not a file", file_path.display()),
            ));
        }

        let mut f = fs::OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .truncate(true)
            .open(file_path)?;
        for entry in &mut self.entrys {
            entry.read_data_to_file(&mut f)?;
        }

        Ok(())
    }
}
