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
//! base
#![allow(missing_docs)]

use crate::config::{TLV_MIN_LEN, TLV_TAG_LEN, TLV_VAL_INVALID_LEN, TLV_VAL_LEN, TLV_VAL_MAXLEN};
use libc::{c_char, c_int};
use std::collections::HashMap;
use std::{env, ffi::CString};

extern "C" {
    fn ProgramMutex(procname: *const c_char, checkOrNew: bool, tmpDir: *const c_char) -> c_int;
}

pub struct Base {}

pub static GLOBAL_SERVER_NAME: &str = "HDCServer";

impl Base {
    pub fn split_command_to_args(command_line: &String) -> (Vec<String>, u32) {
        let mut argv: Vec<String> = Vec::new();
        let mut argc = 0;
        let _a = 0;
        let mut is_quoted = false;
        let mut is_text = false;
        let mut is_space = true;

        let len = command_line.len();
        if len < 1 {
            return (Vec::new(), 0);
        }
        argv.push(String::new());
        for _a in command_line.chars() {
            if is_quoted {
                if _a == '\"' {
                    is_quoted = false;
                } else {
                    argv.last_mut().unwrap().push(_a);
                }
            } else {
                match _a {
                    '\"' => {
                        is_quoted = true;
                        is_text = true;
                        if is_space {
                            argc += 1;
                        }
                        is_space = false;
                    }
                    x if x == ' ' || x == '\t' || x == '\n' || x == '\r' => {
                        if is_text {
                            argv.push(String::new());
                        }
                        is_text = false;
                        is_space = true;
                    }
                    _ => {
                        is_text = true;
                        if is_space {
                            argc += 1;
                        }
                        argv.last_mut().unwrap().push(_a);
                        is_space = false;
                    }
                }
            }
        }

        (argv, argc)
    }

    pub fn get_path_sep() -> char {
        if cfg!(target_os = "windows") {
            '\\'
        } else {
            '/'
        }
    }

    pub fn get_char(str: &str, index: usize) -> char {
        str.chars().nth(index).unwrap()
    }

    pub fn is_absolute_path(path: &str) -> bool {
        if cfg!(target_os = "windows") {
            let tmp = path.to_lowercase();
            let p = tmp.as_str();
            p.len() >= 3
                && (Self::get_char(p, 0) >= 'a' && Self::get_char(p, 0) <= 'z')
                && Self::get_char(p, 1) == ':'
                && Self::get_char(p, 2) == '\\'
        } else {
            path.starts_with('/')
        }
    }

    pub fn get_file_name(s: &mut String) -> Option<String> {
        let temp = s.to_owned();
        let chars: std::str::Chars<'_> = temp.chars();
        let mut result = String::new();
        let mut len = (chars.clone().count() - 1) as i32;
        while len >= 0 && chars.clone().nth(len as usize) == Some(Self::get_path_sep()) {
            len -= 1;
        }
        let begin = len;
        while len >= 0 && chars.clone().nth(len as usize) != Some(Self::get_path_sep()) {
            len -= 1;
        }
        for i in (len + 1) as usize..(begin + 1) as usize {
            result.push(chars.clone().nth(i).unwrap());
        }
        Some(result)
    }

    pub fn extract_relative_path(_cwd: &str, mut _path: &str) -> String {
        if !Base::is_absolute_path(_path) {
            let mut path2 = _cwd.to_owned();
            path2.push_str(_path);
            return path2;
        }
        _path.to_owned()
    }

    pub fn combine(src1: String, src2: String) -> String {
        let path_sep = Base::get_path_sep();
        let mut list1: Vec<&str> = src1.split(path_sep).collect();
        let mut list2: Vec<&str> = src2.split(path_sep).collect();
        // Remove empty strings from the beginning and end of the list
        list1.dedup_by(|a, b| a.is_empty() && b.is_empty());
        list2.dedup_by(|a, b| a.is_empty() && b.is_empty());
        // If list1 is empty, return list2 directly
        if list1.is_empty() {
            return list2.join(path_sep.to_string().as_str());
        }
        // Try to match from the end of list2 to list1
        let mut i = list2.len();
        while i > 0 {
            i -= 1;
            if list1.ends_with(&list2[0..i + 1]) {
                // Remove the matched part
                list2.drain(0..i + 1);
                break;
            }
        }
        // If list2 starts with a path separator and list1 is not empty, remove the separator.
        if !list2.is_empty() && list2[0].starts_with(path_sep) && !list1.is_empty() {
            list2.remove(0);
        }
        let mut combined = list1;
        combined.extend(list2);
        let result = combined.join(path_sep.to_string().as_str());
        result
    }

    pub fn program_mutex(procname: &str, check_or_new: bool) -> bool {
        let temp_path = env::temp_dir();
        let temp_dir = temp_path.display().to_string();
        let ret = unsafe {
            let procname_cstr = CString::new(procname).unwrap();
            let temp_dir_cstr = CString::new(temp_dir).unwrap();
            ProgramMutex(procname_cstr.as_ptr(), check_or_new, temp_dir_cstr.as_ptr())
        };

        matches!(ret, 0)
    }
    // first 16 bytes is tag
    // second 16 bytes is length
    // flow the value
    pub fn tlv_append(mut tlv: String, tag: &str, val: &str) -> String {
        let tlen = tag.len();
        if tlen == 0 || tlen > TLV_TAG_LEN {
            return "".to_string();
        }
        let vlen = val.len();
        if vlen > TLV_VAL_LEN {
            return "".to_string();
        }

        // append tag
        tlv.push_str(tag);
        tlv.push_str(&" ".repeat(TLV_TAG_LEN - tlen));
        // append len
        let svlen = val.len().to_string();
        tlv.push_str(svlen.as_str());
        tlv.push_str(&" ".repeat(TLV_VAL_LEN - svlen.len()));
        // append value
        tlv.push_str(val);
        tlv
    }
    pub fn tlv_to_stringmap(tlv: &str) -> Option<HashMap<&str, &str>> {
        let mut cur_index = 0;
        let mut tlvmap: HashMap<&str, &str> = HashMap::<&str, &str>::new();
        while tlv.len() >= TLV_MIN_LEN && tlv.len() > cur_index {
            // get tag
            let Some(tag) = tlv.get(cur_index..(cur_index + TLV_TAG_LEN)) else {
                return None;
            };
            let tag = tag.trim();
            cur_index += TLV_TAG_LEN;
            // get len
            let Some(svlen) = tlv.get(cur_index..(cur_index + TLV_VAL_LEN)) else {
                return None;
            };
            let svlen = svlen.trim();
            cur_index += TLV_VAL_LEN;
            let vlen = svlen.parse::<usize>().unwrap_or(TLV_VAL_INVALID_LEN);
            if vlen > TLV_VAL_MAXLEN || vlen > tlv.len() {
                return None;
            }
            // get value
            let Some(val) = tlv.get(cur_index..(cur_index + vlen)) else {
                return None;
            };
            let val = val.trim();
            cur_index += vlen;

            tlvmap.insert(tag, val);
        }
        Some(tlvmap)
    }
}
