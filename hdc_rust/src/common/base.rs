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

pub struct Base {}

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

    fn convert(list1: Vec<&str>) -> String {
        let split = Base::get_path_sep();
        let mut result = split.to_string();
        let mut i = 0;
        for item in &list1 {
            result.push_str(item);
            i += 1;
            if i < list1.len() {
                result.push(split);
            }
        }
        result
    }

    pub fn combine(src1: String, src2: String) -> String {
        let split = Base::get_path_sep();
        let split_str = split.to_string();
        let mut list1: Vec<_> = src1.split(&split_str).collect();
        while list1[0].is_empty() {
            list1.remove(0);
        }

        while list1[list1.len() - 1].is_empty() {
            list1.remove(list1.len() - 1);
        }

        let mut list2: Vec<_> = src2.split(&split_str).collect();

        while list2[0].is_empty() {
            list2.remove(0);
        }

        while list2[list2.len() - 1].is_empty() {
            list2.remove(list2.len() - 1);
        }

        let mut i = list2.len() - 1;
        loop {
            if i == 0 {
                if list1.last().unwrap() == &list2[0] {
                    list2.remove(0);
                }
                list1.append(&mut list2);
                return Self::convert(list1);
            }
            if list1.ends_with(&list2[0..i]) {
                for _j in 0..i {
                    list2.remove(0);
                }
                list1.append(&mut list2);
                return Self::convert(list1);
            } else {
                if i == 0 {
                    break;
                }
                i -= 1;
            }
        }
        if !list2.is_empty() {
            list1.append(&mut list2);
        }
        Self::convert(list1)
    }
}
