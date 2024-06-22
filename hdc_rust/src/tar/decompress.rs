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
//! decompress

use std::{
    fs,
    io::{self, Read},
    path::Path,
};

use crate::tar::{entry::Entry, header};
#[allow(unused)]
use crate::utils::hdc_log::*;

/// Decomposes the tar package
pub struct Decompress {
    entrys: Vec<Entry>,
}

impl Decompress {
    /// Decomposes tar
    /// path: tar path
    pub fn file(path: &str) -> Result<Decompress, io::Error> {
        match fs::metadata(path) {
            Ok(metadata) => {
                let file_size = metadata.len();
                if file_size == 0 || file_size % header::HEADER_LEN as u64 != 0 {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        format!("{path} is not tar file"),
                    ));
                }
            }
            Err(e) => return Err(e),
        };
        let mut f = fs::File::open(path)?;
        let mut buff = [0u8; header::HEADER_LEN as usize];

        let mut decompress = Self { entrys: Vec::new() };

        let mut entry = None;
        loop {
            match f.read(&mut buff)? {
                512 => {
                    if entry.is_none() {
                        if let Ok(p_entry) = Entry::create_from_raw_data(&buff) {
                            if p_entry.is_finish() {
                                decompress.entrys.push(p_entry);
                            } else {
                                entry = Some(p_entry);
                            }
                        }
                        continue;
                    }
                    let p_entry = entry.as_mut().unwrap();
                    p_entry.add_data(&buff);
                    if p_entry.is_finish() {
                        decompress.entrys.push(entry.unwrap());
                        entry = None;
                    }
                }
                0 => break,
                n => {
                    crate::error!("read error n {n}");
                    break;
                }
            };
        }

        Ok(decompress)
    }

    /// 将文件解压到文件夹，传入路径为空则是当前文件夹
    pub fn decompress(&self, prefix: &str) -> io::Result<()> {
        let prefix = if !prefix.is_empty() { prefix } else { "./" };

        let prefix_path = Path::new(prefix);

        if prefix_path.exists() {
            if prefix_path.is_file() {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    format!("{} is not a dir", prefix),
                ));
            }
        } else {
            crate::debug!("need create dir {}", prefix);
            fs::create_dir_all(prefix)?;
        }

        for entry in &self.entrys {
            if !entry.is_finish() {
                crate::error!("file data is not load");
                continue;
            }
            if let Err(e) = entry.write_to_file(prefix_path) {
                crate::error!("entry.write_to_file failed: {}", e);
            }
        }

        Ok(())
    }
}
