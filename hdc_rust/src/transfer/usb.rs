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
//! usb
#![allow(missing_docs)]

use super::base;

use crate::config;
use crate::serializer;
use crate::serializer::native_struct::UsbHead;
use crate::serializer::pack_struct::UsbHeadPack;
use crate::serializer::serialize::Serialization;
use crate::serializer::serialize::SerializedBuffer;
use crate::utils;
#[allow(unused)]
use crate::utils::hdc_log::*;

#[cfg(not(target_os = "windows"))]
use std::ffi::{CStr, CString};
use std::io::{self, Error, ErrorKind};

#[repr(C)]
pub struct PersistBuffer {
    pub ptr: *const libc::c_char,
    pub size: libc::c_ulonglong,
}

pub fn buf_to_vec(buf: PersistBuffer) -> Vec<u8> {
    let slice =
        unsafe { std::slice::from_raw_parts(buf.ptr as *const libc::c_uchar, buf.size as usize) };
    slice.to_vec()
}

#[allow(unused)]
extern "C" {
    fn access(_name: *const libc::c_char, _type: i32) -> i32;
    fn free(ptr: *const libc::c_void);

    fn ConfigEpPointEx(path: *const libc::c_char) -> i32;
    fn OpenEpPointEx(path: *const libc::c_char) -> i32;
    fn CloseUsbFdEx(fd: i32) -> i32;
    fn CloseEndPointEx(bulkIn: i32, bulkOut: i32, ctrlEp: i32, closeCtrlEp: u8);
    #[cfg(not(target_os = "windows"))]
    fn WriteUsbDevEx(bulkOut: i32, buf: SerializedBuffer) -> i32;
    #[cfg(not(target_os = "windows"))]
    fn ReadUsbDevEx(bulkIn: i32) -> PersistBuffer;
    fn GetDevPathEx(path: *const libc::c_char) -> *const libc::c_char;

    fn SerializeUsbHead(value: *const UsbHeadPack) -> SerializedBuffer;
    fn ParseUsbHead(value: *mut UsbHeadPack, buf: SerializedBuffer) -> libc::c_uchar;
}

#[cfg(not(target_os = "windows"))]
pub fn usb_init() -> io::Result<(i32, i32, i32)> {
    crate::info!("opening usb fd...");
    let path = CString::new(config::USB_FFS_BASE).unwrap();

    let base_path = unsafe {
        let p = GetDevPathEx(path.as_ptr());
        let c_str = CStr::from_ptr(p);
        c_str.to_str().unwrap().to_string()
    };
    // let c_str: &CStr = unsafe { CStr::from_ptr(p) };
    // c_str.to_str().unwrap().to_string()
    // let base_path = serializer::ptr_to_string(unsafe { GetDevPathEx(path.as_ptr()) });
    let ep0 = CString::new(base_path.clone() + "/ep0").unwrap();
    let ep1 = CString::new(base_path.clone() + "/ep1").unwrap();
    let ep2 = CString::new(base_path + "/ep2").unwrap();
    if unsafe { access(ep0.as_ptr(), 0) } != 0 {
        return Err(utils::error_other("cannot access usb path".to_string()));
    }

    let config_fd = unsafe { ConfigEpPointEx(ep0.as_ptr()) };
    if config_fd < 0 {
        return Err(utils::error_other("cannot open usb ep0".to_string()));
    }

    let bulkin_fd = unsafe { OpenEpPointEx(ep1.as_ptr()) };
    if bulkin_fd < 0 {
        return Err(utils::error_other("cannot open usb ep1".to_string()));
    }

    let bulkout_fd = unsafe { OpenEpPointEx(ep2.as_ptr()) };
    if bulkout_fd < 0 {
        return Err(utils::error_other("cannot open usb ep2".to_string()));
    }

    crate::info!("usb fd: {config_fd}, {bulkin_fd}, {bulkout_fd}");

    Ok((config_fd, bulkin_fd, bulkout_fd))
}

#[cfg(not(target_os = "windows"))]
pub fn usb_close(config_fd: i32, bulkin_fd: i32, bulkout_fd: i32) {
    crate::info!("closing usb fd...");
    unsafe {
        CloseUsbFdEx(config_fd);
        CloseUsbFdEx(bulkin_fd);
        CloseUsbFdEx(bulkout_fd);
    }
}

pub struct UsbReader {
    pub fd: i32,
}
pub struct UsbWriter {
    pub fd: i32,
}

impl base::Reader for UsbReader {
    // 屏蔽window编译报错
    #[cfg(not(target_os = "windows"))]
    fn read_frame(&self, expected_size: usize) -> io::Result<Vec<u8>> {
        let buf = unsafe { ReadUsbDevEx(self.fd) };
        match buf.size.cmp(&expected_size.try_into().expect("transfer usize to u64")) {
            std::cmp::Ordering::Equal => Ok(buf_to_vec(buf)),
            std::cmp::Ordering::Greater => Ok(buf_to_vec(buf).split_at(expected_size).0.to_vec()),
            std::cmp::Ordering::Less => Err(
                utils::error_other(
                    format!(
                        "usb read error, usb read result size: {:} is not equal to expected_size: {}",
                        buf.size,
                        expected_size,
                    )
                )
            )
        }
    }

    // 屏蔽window编译报错
    #[cfg(target_os = "windows")]
    fn read_frame(&self, _expected_size: usize) -> io::Result<Vec<u8>> {
        Err(utils::error_other("usb read error".to_string()))
    }

    fn check_protocol_head(&mut self) -> io::Result<(u32, u32)> {
        let buf = self.read_frame(serializer::USB_HEAD_SIZE)?;
        if buf[..config::USB_PACKET_FLAG.len()] != config::USB_PACKET_FLAG[..] {
            return Err(Error::new(
                ErrorKind::Other,
                format!("USB_PACKET_FLAG incorrect, content: {:#?}", buf),
            ));
        }
        let mut head = serializer::native_struct::UsbHead::default();

        if let Err(e) = head.parse(buf) {
            crate::warn!("parse usb head error: {}", e.to_string());
            return Err(e);
        }
        Ok((u32::from_be(head.data_size), 0))
    }
}

impl base::Writer for UsbWriter {
    // 屏蔽window编译报错
    #[cfg(not(target_os = "windows"))]
    fn write_all(&self, data: Vec<u8>) -> io::Result<i32> {
        let buf = SerializedBuffer {
            ptr: data.as_ptr() as *const libc::c_char,
            size: data.len() as u64,
        };
        let ret = unsafe { WriteUsbDevEx(self.fd, buf) } as i32;
        if ret < 0 {
            Err(utils::error_other("usb write failed".to_string()))
        } else {
            Ok(ret)
        }
    }

    // 屏蔽window编译报错
    #[cfg(target_os = "windows")]
    fn write_all(&self, _data: Vec<u8>) -> io::Result<i32> {
        Ok(0)
    }
}

pub fn build_header(session_id: u32, option: u8, length: usize) -> Vec<u8> {
    UsbHead {
        session_id: u32::to_be(session_id),
        flag: [config::USB_PACKET_FLAG[0], config::USB_PACKET_FLAG[1]],
        option,
        data_size: u32::to_be(length as u32),
    }
    .serialize()
}
