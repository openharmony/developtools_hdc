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
//! host_usb
#![allow(missing_docs)]
#[cfg(feature = "host")]
extern crate ylong_runtime_static as ylong_runtime;
use crate::config;
use crate::serializer;
use crate::serializer::native_struct::UsbHead;
use crate::serializer::serialize::Serialization;
use crate::serializer::serialize::SerializedBuffer;
use crate::transfer::base;
use crate::utils;
#[allow(unused)]
use crate::utils::hdc_log::*;

use std::io::{self, Error, ErrorKind};
use std::collections::HashMap;
use std::sync::Arc;
use ylong_runtime::sync::mpsc;
use ylong_runtime::sync::Mutex;
use ylong_runtime::sync::RwLock;
use crate::config::TaskMessage;
use crate::config::ConnectType;
use crate::transfer::buffer::ConnectTypeMap;
use crate::transfer::base::Reader;
use crate::transfer::base::Writer;
use std::string::FromUtf8Error;
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

extern "C" {
    fn InitHostUsb() -> *mut libc::c_void;
    fn GetReadyUsbDevice(ptr: *mut libc::c_void) -> PersistBuffer;
    fn OnDeviceConnected(ptr: *mut libc::c_void, connect_key: *mut libc::c_char, len: i32, connectSuccess: bool);
    fn WriteUsb(
        ptr: *mut libc::c_void,
        connect_key: *mut libc::c_char,
        len: i32,
        buf: SerializedBuffer,
    ) -> libc::c_int;
    fn ReadUsb(
        ptr: *mut libc::c_void,
        connect_key: *mut libc::c_char,
        len: i32,
        excepted_size: i32,
    ) -> PersistBuffer;
    fn CancelUsbIo(ptr: *mut libc::c_void, connect_key: *mut libc::c_char, len: i32);
    fn Stop(ptr: *mut libc::c_void) -> bool;
}

pub fn init_host_usb() -> *mut libc::c_void {
    unsafe { InitHostUsb() }
}

pub fn get_ready_usb_devices_string(ptr: u64) -> Result<String, FromUtf8Error> {
    let buf = get_ready_usb_devices(ptr);
    String::from_utf8(buf_to_vec(buf))
}

pub fn get_ready_usb_devices(ptr: u64) -> PersistBuffer {
    unsafe { GetReadyUsbDevice(ptr as *mut libc::c_void) }
}

pub fn on_device_connected(ptr: u64, connect_key: String, connect_success: bool) {
    unsafe {
        OnDeviceConnected(
            ptr as *mut libc::c_void,
            connect_key.as_str().as_ptr() as *mut libc::c_char,
            connect_key.len() as i32,
            connect_success,
        );
    }
}

pub fn write_usb(ptr: u64, connect_key: String, buf: SerializedBuffer) -> i32 {
    unsafe {
        WriteUsb(
            ptr as *mut libc::c_void,
            connect_key.as_str().as_ptr() as *mut libc::c_char,
            connect_key.len() as i32,
            buf,
        )
    }
}

pub fn read_usb(ptr: u64, connect_key: String, excepted_size: i32) -> PersistBuffer {
    unsafe {
        ReadUsb(
            ptr as *mut libc::c_void,
            connect_key.as_str().as_ptr() as *mut libc::c_char,
            connect_key.len() as i32,
            excepted_size,
        )
    }
}

pub fn cancel_usb_io(ptr: u64, connect_key: String) {
    unsafe {
        CancelUsbIo(
            ptr as *mut libc::c_void,
            connect_key.as_str().as_ptr() as *mut libc::c_char,
            connect_key.len() as i32,
        );
    }
}

pub fn stop(ptr: u64) {
    unsafe {
        Stop(ptr as *mut libc::c_void);
    }
}

pub struct HostUsbReader {
    pub connect_key: String,
    pub ptr: u64,
}
pub struct HostUsbWriter {
    pub connect_key: String,
    pub ptr: u64,
}

impl base::Reader for HostUsbReader {
    fn read_frame(&self, expected_size: usize) -> io::Result<Vec<u8>> {
        let buf = read_usb(self.ptr, self.connect_key.clone(), expected_size as i32);
        if buf.size == 0 {
            crate::warn!("usb read result < 0");
            return Err(utils::error_other("usb read error".to_string()));
        }

        Ok(buf_to_vec(buf))
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

impl base::Writer for HostUsbWriter {
    fn write_all(&self, data: Vec<u8>) -> io::Result<i32> {
        let buf = SerializedBuffer {
            ptr: data.as_ptr() as *const libc::c_char,
            size: data.len() as u64,
        };
        let ret = write_usb(self.ptr, self.connect_key.clone(), buf);
        if ret < 0 {
            Err(utils::error_other("usb write failed".to_string()))
        } else {
            Ok(ret)
        }
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

pub async fn recv_channel_message(rd: &mut HostUsbReader) -> io::Result<Vec<u8>> {
    let data = rd.read_frame(4)?;
    let expected_size = u32::from_be_bytes(data.try_into().unwrap());
    rd.read_frame(expected_size as usize)
}


pub fn start_recv(
    ptr: u64,
    connect_key: String,
    _session_id: u32,
) -> mpsc::BoundedReceiver<(TaskMessage, u32)> {
    let (tx, rx) = mpsc::bounded_channel::<(TaskMessage, u32)>(config::USB_QUEUE_LEN);
    ylong_runtime::spawn(async move {
        let mut rd: HostUsbReader = HostUsbReader { connect_key, ptr };
        loop {
            if let Err(e) = base::unpack_task_message(&mut rd, tx.clone()) {
                crate::warn!("unpack task failed: {}, reopen fd...", e.to_string());
                break;
            }
        }
    });
    rx
}

pub fn start_recv_once(
    ptr: u64,
    connect_key: String,
    _session_id: u32,
) -> mpsc::BoundedReceiver<(TaskMessage, u32)> {
    let (tx, rx) = mpsc::bounded_channel::<(TaskMessage, u32)>(config::USB_QUEUE_LEN);
    ylong_runtime::spawn(async move {
        let mut rd: HostUsbReader = HostUsbReader { connect_key, ptr };
        if let Err(e) = base::unpack_task_message(&mut rd, tx.clone()) {
            crate::warn!("unpack task failed: {}, reopen fd...", e.to_string());
        }
    });
    rx
}


type HostUsbWriter_ = Arc<Mutex<HostUsbWriter>>;
type HostUsbMap_ = Arc<RwLock<HashMap<u32, HostUsbWriter_>>>;

pub struct HostUsbMap {}
impl HostUsbMap {
    fn get_instance() -> HostUsbMap_ {
        static mut USB_MAP: Option<HostUsbMap_> = None;
        unsafe {
            USB_MAP
                .get_or_insert_with(|| Arc::new(RwLock::new(HashMap::new())))
                .clone()
        }
    }

    #[allow(unused)]
    pub async fn put(session_id: u32, data: TaskMessage) -> io::Result<()> {
        let body = serializer::concat_pack(data);
        crate::debug!("transfer put data {:?}", body);
        let head = build_header(session_id, 1, body.len());
        let tail = build_header(session_id, 0, 0);

        let instance = Self::get_instance();
        let map: ylong_runtime::sync::RwLockReadGuard<
            '_,
            HashMap<u32, Arc<Mutex<HostUsbWriter>>>,
        > = instance.read().await;
        let arc_wr = map.get(&session_id).unwrap();
        let mut wr = arc_wr.lock().await;
        wr.write_all(head)?;
        wr.write_all(body)?;
        wr.write_all(tail)?;
        Ok(())
    }

    #[allow(unused)]
    pub async fn send_channel_message(channel_id: u32, buf: Vec<u8>) -> io::Result<()> {
        crate::trace!("send channel msg: {:#?}", buf.clone());
        let send = [
            u32::to_be_bytes(buf.len() as u32).as_slice(),
            buf.as_slice(),
        ]
        .concat();
        let instance = Self::get_instance();
        let map = instance.read().await;
        if let Some(guard) = map.get(&channel_id) {
            let mut wr = guard.lock().await;
            let _ = wr.write_all(send);
            return Ok(());
        }
        Err(Error::new(ErrorKind::NotFound, "channel not found"))
    }

    #[allow(unused)]
    pub async fn start(session_id: u32, wr: HostUsbWriter) {
        let buffer_map = Self::get_instance();
        let mut map = buffer_map.write().await;
        let arc_wr = Arc::new(Mutex::new(wr));
        map.insert(session_id, arc_wr);
        ConnectTypeMap::put(
            session_id,
            ConnectType::HostUsb("some_mount_point".to_string()),
        )
        .await;
    }

    #[allow(unused)]
    pub async fn end(id: u32) {
        let instance = Self::get_instance();
        let mut map = instance.write().await;
        let _ = map.remove(&id);
        ConnectTypeMap::del(id).await;
    }
}

