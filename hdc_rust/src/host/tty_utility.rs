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
// !tty_utility.rs: functions for shell input convert

#[cfg(target_os = "windows")]
use std::collections::HashMap;

#[cfg(target_os = "windows")]
extern "C" {
    fn getch() -> libc::c_int;
}

#[cfg(target_os = "windows")]
const UNICODE_ONE_BYTES: i32 = 0b00000000;
#[cfg(target_os = "windows")]
const UNICODE_ONE_BYTES_JUDGE: i32 = 0b10000000;
#[cfg(target_os = "windows")]
const UNICODE_TWO_BYTES: i32 = 0b11000000;
#[cfg(target_os = "windows")]
const UNICODE_THREE_BYTES: i32 = 0b11100000;
#[cfg(target_os = "windows")]
const UNICODE_FOUR_BYTES: i32 = 0b11110000;

#[cfg(target_os = "windows")]
const VK_UP: i32 = 0x48;

#[cfg(target_os = "windows")]
lazy_static! {
    static ref VIRTIAL_KEY_MAP: HashMap<i32, String> = {
        let mut map = HashMap::new();

        map.insert(VK_UP, "[A".to_string());
        map
    };
}

#[cfg(target_os = "windows")]
pub fn convert_to_control_code() -> Vec<u8> {
    // 获取控制字符后的字符
    let control_char = unsafe { getch() };

    let mut unicode_byte: Vec<u8> = Vec::new();
    // linux下的控制码以33开头
    unicode_byte.push(0x1b_u8);
    // 根据VIRTIAL_KEY_MAP中保存的对应关系把win下的vitual key转换成linux对用的按键码
    match VIRTIAL_KEY_MAP.get(&control_char) {
        Some(virtual_key_string) => {
            for (_index, c) in virtual_key_string.chars().enumerate() {
                unicode_byte.push(c as u8);
            }
        }
        None => hdc::info!("current control code is not support now"),
    }

    unicode_byte
}

#[cfg(target_os = "windows")]
fn get_unicode_len(input_char: i32) -> u32 {
    let mut len = 0;
    if input_char & UNICODE_ONE_BYTES_JUDGE == UNICODE_ONE_BYTES {
        len = 1;
    }
    if input_char & UNICODE_TWO_BYTES == UNICODE_TWO_BYTES {
        len = 2;
    }
    if input_char & UNICODE_THREE_BYTES == UNICODE_THREE_BYTES {
        len = 3;
    }
    if input_char & UNICODE_FOUR_BYTES == UNICODE_FOUR_BYTES {
        len = 4;
    }
    len
}

#[cfg(target_os = "windows")]
// 通过第一个字符判断unicode长度，并读取组装完成的unicode
pub fn unicode_assemble(first_char: i32) -> Vec<u8> {
    let mut len = get_unicode_len(first_char);
    hdc::info!("unicode bytes len is {:?}", len);

    let mut unicode_byte: Vec<u8> = Vec::new();
    unicode_byte.push(first_char as u8);
    if len > 1 {
        len -= 1;
        while len > 0 {
            let left_char = unsafe { getch() };
            unicode_byte.push(left_char as u8);
            len -= 1;
        }
    }

    unicode_byte
}
