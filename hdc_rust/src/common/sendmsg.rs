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
//! sendmsg
#![allow(missing_docs)]
#[cfg(not(target_os = "windows"))]
use std::ffi::c_int;

#[cfg(not(target_os = "windows"))]
extern "C" {
    fn SendMsg(socket_fd: c_int, fd: c_int, data: *mut libc::c_char, size: c_int) -> c_int;
}

#[cfg(not(target_os = "windows"))]
pub fn send_msg(socket_fd: i32, fd: i32, data: &[u8]) -> i32 {
    unsafe {
        SendMsg(
            socket_fd,
            fd,
            data.as_ptr() as *mut libc::c_char,
            data.len() as i32,
        )
    }
}
