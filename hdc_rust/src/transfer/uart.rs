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
//! uart
#![allow(missing_docs)]

use super::base;

use super::uart_wrapper;
use crate::config;
use crate::serializer;
#[allow(unused)]
use crate::serializer::native_struct::UartHead;
use crate::serializer::serialize::Serialization;
use crate::serializer::serialize::SerializedBuffer;
use crate::utils;
#[allow(unused)]
use crate::utils::hdc_log::*;

use std::ffi::CString;
use std::io::{self, Error, ErrorKind};

#[allow(unused)]
extern "C" {
    fn access(_name: *const libc::c_char, _type: i32) -> i32;
    fn free(ptr: *const libc::c_void);

    fn GetUartSpeedExt(speed: i32) -> i32;
    fn GetUartBitsExt(bits: i32) -> i32;
    fn OpenSerialPortExt(port: *const libc::c_char) -> i32;
    fn SetSerialExt(fd: i32, speed: i32, bits: i32, event: u8, stop: i32) -> i32;
    fn ReadUartDevExt(fd: i32, size: i32) -> SerializedBuffer;
    fn WriteUartDevExt(fd: i32, buf: SerializedBuffer) -> i32;
    fn CloseSerialPortExt(fd: i32) -> u8;
}

pub fn uart_init() -> io::Result<i32> {
    let name = CString::new(config::UART_NODE).unwrap();
    let fd = unsafe {
        if access(name.as_ptr(), 0) != 0 {
            return Err(utils::error_other("cannot access uart node".to_string()));
        }
        let fd = OpenSerialPortExt(name.as_ptr());
        if fd < 0 {
            return Err(utils::error_other("cannot open uart node".to_string()));
        }
        if SetSerialExt(
            fd,
            config::UART_DEFAULT_BAUD_RATE,
            config::UART_DEFAULT_BITS,
            config::UART_EVENT,
            1,
        ) != 0
        {
            return Err(utils::error_other("set uart config failed".to_string()));
        }
        println!("uart init fd: {fd}");
        fd
    };
    Ok(fd)
}

pub fn uart_close(fd: i32) {
    unsafe {
        CloseSerialPortExt(fd);
    }
}

pub struct UartReader {
    pub fd: i32,
    pub head: Option<UartHead>,
}

pub struct UartWriter {
    pub fd: i32,
}

impl base::Reader for UartReader {
    fn read_frame(&self, expected_size: usize) -> io::Result<Vec<u8>> {
        if expected_size == 0 {
            return Ok(vec![]);
        }
        let mut data = vec![];
        let mut index = 0;
        while index < expected_size {
            crate::trace!("before read {index} / {expected_size}");
            let buf = unsafe {
                let recv = ReadUartDevExt(self.fd, (expected_size - index) as i32);
                let slice = std::slice::from_raw_parts(
                    recv.ptr as *const libc::c_uchar,
                    recv.size as usize,
                );
                index += recv.size as usize;
                slice.to_vec()
            };
            data = [data, buf].concat();
        }
        Ok(data)
    }

    fn check_protocol_head(&mut self) -> io::Result<(u32, u32)> {
        let buf = self.read_frame(serializer::UART_HEAD_SIZE)?;
        if buf[..config::PACKET_FLAG.len()] != config::PACKET_FLAG[..] {
            return Err(Error::new(
                ErrorKind::Other,
                format!("uart PACKET_FLAG incorrect, content: {:#?}", buf),
            ));
        }
        let mut head = serializer::native_struct::UartHead::default();

        if let Err(e) = head.parse(buf) {
            println!("parse uart head error {:#?}", e);
            log::warn!("parse uart head error: {}", e.to_string());
            return Err(e);
        }

        self.head = Some(head.clone());

        Ok((head.data_size, head.package_index))
    }

    #[allow(unused)]
    fn process_head(&self) {
        let head = self.head.clone();
        if let Some(head) = head {
            ylong_runtime::block_on(async move {
                uart_wrapper::on_read_head(head).await;
            });
        }
    }
}

impl base::Writer for UartWriter {
    fn write_all(&self, data: Vec<u8>) -> io::Result<()> {
        let buf = SerializedBuffer {
            ptr: data.as_ptr() as *const libc::c_char,
            size: data.len() as u64,
        };
        println!("write all start, fd:{}...", self.fd);
        let write_count = unsafe { WriteUartDevExt(self.fd, buf) };
        println!("write count:{}", write_count);
        if write_count < 0 {
            Err(utils::error_other("uart write failed".to_string()))
        } else {
            Ok(())
        }
    }
}

pub fn build_header(session_id: u32, option: u16, length: usize, package_index: u32) -> Vec<u8> {
    UartHead {
        session_id: u32::to_le(session_id),
        flag: [config::PACKET_FLAG[0], config::PACKET_FLAG[1]],
        option,
        data_size: u32::to_le(length as u32),
        package_index,
        data_checksum: 0,
        head_checksum: 0,
    }
    .serialize()
}

pub fn build_header_obj(
    session_id: u32,
    option: u16,
    length: usize,
    package_index: u32,
) -> UartHead {
    UartHead {
        session_id: u32::to_le(session_id),
        flag: [config::PACKET_FLAG[0], config::PACKET_FLAG[1]],
        option,
        data_size: u32::to_le(length as u32),
        package_index,
        data_checksum: 0,
        head_checksum: 0,
    }
}
