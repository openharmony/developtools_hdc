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
//! bridge

#[allow(unused)]
use crate::config::*;
#[allow(unused)]
use crate::serializer;
#[allow(unused)]
use crate::serializer::serialize::Serialization;
use crate::serializer::serialize::SerializedBuffer;
use crate::transfer::base;
use crate::transfer::base::Writer;
use crate::transfer::buffer::ConnectTypeMap;
use crate::utils;
#[allow(unused)]
use crate::utils::hdc_log::*;
use std::collections::HashMap;
#[allow(unused)]
use std::io::{self, Error, ErrorKind};
use std::sync::Arc;
use ylong_runtime::sync::{Mutex, RwLock};

#[repr(C)]
pub struct PersistBuffer {
    pub ptr: *const libc::c_char,
    pub size: libc::c_ulonglong,
}

#[allow(unused)]
pub fn buf_to_vec(buf: PersistBuffer) -> Vec<u8> {
    let slice =
        unsafe { std::slice::from_raw_parts(buf.ptr as *const libc::c_uchar, buf.size as usize) };
    slice.to_vec()
}

#[allow(unused)]
extern "C" {
    fn InitBridge() -> *mut libc::c_void;
    fn StartListen(ptr: *mut libc::c_void) -> libc::c_int;
    fn AcceptServerSocketFd(ptr: *mut libc::c_void, pipeFd: i32) -> libc::c_int;
    fn InitClientFd(ptr: *mut libc::c_void, socketFd: i32) -> libc::c_int;
    fn ReadClient(ptr: *mut libc::c_void, fd: i32, excepted_size: i32) -> PersistBuffer;
    fn WriteClient(ptr: *mut libc::c_void, fd: i32, buf: SerializedBuffer) -> libc::c_int;
    fn Stop(ptr: *mut libc::c_void) -> libc::c_int;
}

#[allow(unused)]
pub fn init_bridge() -> *mut libc::c_void {
    unsafe { InitBridge() }
}

#[allow(unused)]
pub fn start_listen(ptr: u64) -> i32 {
    unsafe { StartListen(ptr as *mut libc::c_void) }
}

#[allow(unused)]
pub fn accept_server_socket_fd(ptr: u64, pipe_fd: i32) -> i32 {
    unsafe { AcceptServerSocketFd(ptr as *mut libc::c_void, pipe_fd) }
}

#[allow(unused)]
pub fn init_client_fd(ptr: u64, socket_fd: i32) -> i32 {
    unsafe { InitClientFd(ptr as *mut libc::c_void, socket_fd) }
}

#[allow(unused)]
pub fn write_client(ptr: u64, fd: i32, buf: SerializedBuffer) -> i32 {
    unsafe { WriteClient(ptr as *mut libc::c_void, fd, buf) }
}

#[allow(unused)]
pub fn read_client(ptr: u64, fd: i32, excepted_size: i32) -> PersistBuffer {
    unsafe { ReadClient(ptr as *mut libc::c_void, fd, excepted_size) }
}

#[allow(unused)]
pub fn stop(ptr: u64) -> i32 {
    unsafe { Stop(ptr as *mut libc::c_void) }
}

pub struct BridgeReader {
    pub ptr: u64,
    pub fd: i32,
}
pub struct BridgeWriter {
    pub ptr: u64,
    pub fd: i32,
}

impl base::Reader for BridgeReader {
    fn read_frame(&self, _expected_size: usize) -> io::Result<Vec<u8>> {
        let buf = read_client(self.ptr, self.fd, _expected_size as i32);
        if buf.size == 0 {
            crate::warn!("bridge read result <= 0");
            return Err(utils::error_other("bridge read error".to_string()));
        }

        Ok(buf_to_vec(buf))
    }

    fn check_protocol_head(&mut self) -> io::Result<(u32, u32)> {
        Ok((0, 0))
    }
}

impl base::Writer for BridgeWriter {
    fn write_all(&self, data: Vec<u8>) -> io::Result<i32> {
        let buf = SerializedBuffer {
            ptr: data.as_ptr() as *const libc::c_char,
            size: data.len() as u64,
        };
        let ret = write_client(self.ptr, self.fd, buf);
        if ret <= 0 {
            Err(utils::error_other("usb write failed".to_string()))
        } else {
            Ok(ret)
        }
    }
}

type BridgeWriter_ = Arc<Mutex<BridgeWriter>>;
type BridgeMap_ = Arc<RwLock<HashMap<u32, BridgeWriter_>>>;

pub struct BridgeMap {}
impl BridgeMap {
    fn get_instance() -> BridgeMap_ {
        static mut BRIDGE_MAP: Option<BridgeMap_> = None;
        unsafe {
            BRIDGE_MAP
                .get_or_insert_with(|| Arc::new(RwLock::new(HashMap::new())))
                .clone()
        }
    }

    pub async fn put(session_id: u32, data: TaskMessage) {
        let send = serializer::concat_pack(data);
        let instance = Self::get_instance();
        let map = instance.read().await;
        let Some(arc_wr) = map.get(&session_id) else {
            return;
        };
        let wr = arc_wr.lock().await;
        let _ = wr.write_all(send);
    }

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
            let wr = guard.lock().await;
            let _ = wr.write_all(send);
            return Ok(());
        }
        Err(Error::new(ErrorKind::NotFound, "channel not found"))
    }

    pub async fn start(id: u32, wr: BridgeWriter) {
        let instance = Self::get_instance();
        let mut map = instance.write().await;
        let arc_wr = Arc::new(Mutex::new(wr));
        map.insert(id, arc_wr);
        ConnectTypeMap::put(id, ConnectType::Bridge).await;
    }

    pub async fn end(id: u32) {
        let instance = Self::get_instance();
        let mut map = instance.write().await;
        if let Some(arc_wr) = map.remove(&id) {
            let mut _wr = arc_wr.lock().await;
        }
    }
}
