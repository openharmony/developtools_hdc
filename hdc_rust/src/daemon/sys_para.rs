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

#![allow(missing_docs)]

use hdc::config::*;
use std::ffi::CString;
use crate::utils::hdc_log::*;

extern "C" {
    fn SetParameterEx(key: *const libc::c_char, val: *const libc::c_char) -> libc::c_int;
    fn GetParameterEx(
        key: *const libc::c_char,
        def: *const libc::c_char,
        val: *mut libc::c_char,
        len: libc::c_uint,
    ) -> libc::c_int;
    #[allow(dead_code)]
    fn WaitParameterEx(
        key: *const libc::c_char,
        val: *const libc::c_char,
        timeout: libc::c_int,
    ) -> libc::c_int;
}

pub fn set_dev_item(key: &str, val: &str) -> bool {
    let ckey = CString::new(key).unwrap();
    let cval = CString::new(val).unwrap();

    unsafe {
        let ret = SetParameterEx(ckey.as_ptr(), cval.as_ptr());
        hdc::debug!("set param:{} val:{} ret:{}", key, val, ret);
        ret == 0
    }
}

pub fn get_dev_item(key: &str, def: &str) -> (bool, String) {
    let ckey = CString::new(key).unwrap();
    let cdef = CString::new(def).unwrap();
    let mut out: [u8; HDC_PARAMETER_VALUE_MAX_LEN] = [0; HDC_PARAMETER_VALUE_MAX_LEN];

    unsafe {
        let bytes = GetParameterEx(
            ckey.as_ptr(),
            cdef.as_ptr(),
            out.as_mut_ptr() as *mut libc::c_char,
            512,
        );
        let output = String::from_utf8(out.to_vec()).unwrap().trim().to_string();
        let (val, _) = output.split_at(bytes as usize);
        (bytes >= 0, val.to_string())
    }
}

#[allow(dead_code)]
pub fn wait_dev_item(key: &str, val: &str, timeout: i32) -> bool {
    let ckey = CString::new(key).unwrap();
    let cval = CString::new(val).unwrap();

    unsafe { WaitParameterEx(ckey.as_ptr(), cval.as_ptr(), timeout) == 0 }
}
