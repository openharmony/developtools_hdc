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
//! serialize
#![allow(missing_docs)]

extern crate libc;

use crate::serializer::native_struct::{FileMode, TransferConfig, TransferPayload};
use crate::serializer::native_struct::{PayloadHead, PayloadProtect, SessionHandShake};
use crate::serializer::pack_struct::*;

use std::ffi::CStr;
use std::ffi::CString;
use std::io::{self, Error, ErrorKind};

use super::native_struct::UsbHead;

// use tokio::io::Result;

#[repr(C)]
pub struct SerializedBuffer {
    pub ptr: *const libc::c_char,
    pub size: libc::c_ulonglong,
}

impl Drop for SerializedBuffer {
    fn drop(&mut self) {
        unsafe {
            free(self.ptr.cast());
        }
    }
}

pub struct RawPayload {
    pub bytes: Vec<u8>,
}

extern "C" {
    fn free(ptr: *const libc::c_void);

    fn SerializeSessionHandShake(value: *const SessionHandShakePack) -> SerializedBuffer;
    fn SerializePayloadProtect(value: *const PayloadProtectPack) -> SerializedBuffer;
    fn SerializeTransferConfig(value: *const TransferConfigPack) -> SerializedBuffer;
    fn SerializeFileMode(value: *const FileModePack) -> SerializedBuffer;
    fn SerializeTransferPayload(value: *const TransferPayloadPack) -> SerializedBuffer;
    fn SerializePayloadHead(value: *const PayloadHeadPack) -> SerializedBuffer;
    fn SerializeUsbHead(value: *const UsbHeadPack) -> SerializedBuffer;

    fn ParseSessionHandShake(
        value: *mut SessionHandShakePack,
        buf: SerializedBuffer,
    ) -> libc::c_uchar;
    fn ParsePayloadProtect(value: *mut PayloadProtectPack, buf: SerializedBuffer) -> libc::c_uchar;
    fn ParseTransferConfig(value: *mut TransferConfigPack, buf: SerializedBuffer) -> libc::c_uchar;
    fn ParseFileMode(value: *mut FileModePack, buf: SerializedBuffer) -> libc::c_uchar;
    fn ParseTransferPayload(
        value: *mut TransferPayloadPack,
        buf: SerializedBuffer,
    ) -> libc::c_uchar;
    fn ParsePayloadHead(value: *mut PayloadHeadPack, buf: SerializedBuffer) -> libc::c_uchar;
    fn ParseUsbHead(value: *mut UsbHeadPack, buf: SerializedBuffer) -> libc::c_uchar;
}

pub trait Serialization {
    fn serialize(&self) -> Vec<u8>;
    fn parse(&mut self, _: Vec<u8>) -> io::Result<()> {
        Ok(())
    }
}

impl Serialization for RawPayload {
    fn serialize(&self) -> Vec<u8> {
        self.bytes.clone()
    }
}

impl Serialization for SessionHandShake {
    fn serialize(&self) -> Vec<u8> {
        let banner = CString::new(self.banner.as_str()).unwrap();
        let connect_key = CString::new(self.connect_key.as_str()).unwrap();
        let buf = CString::new(self.buf.as_str()).unwrap();
        let version = CString::new(self.version.as_str()).unwrap();
        let pack = SessionHandShakePack {
            banner: banner.as_ptr(),
            auth_type: self.auth_type,
            session_id: self.session_id,
            connect_key: connect_key.as_ptr(),
            buf: buf.as_ptr(),
            version: version.as_ptr(),
        };
        let buf = unsafe { SerializeSessionHandShake(&pack) };
        buf_to_vec(buf)
    }

    fn parse(&mut self, input: Vec<u8>) -> io::Result<()> {
        let mut pack = SessionHandShakePack {
            banner: std::ptr::null(),
            auth_type: 0,
            session_id: 0,
            connect_key: std::ptr::null(),
            buf: std::ptr::null(),
            version: std::ptr::null(),
        };

        let buf = SerializedBuffer {
            ptr: input.as_ptr() as *const libc::c_char,
            size: input.len() as u64,
        };
        if unsafe { ParseSessionHandShake(&mut pack, buf) } == 0 {
            return Err(Error::new(
                ErrorKind::Other,
                "cffi ParseSessionHandShake failed",
            ));
        }

        self.banner = ptr_to_string(pack.banner);
        self.auth_type = pack.auth_type;
        self.session_id = pack.session_id;
        self.connect_key = ptr_to_string(pack.connect_key);
        self.buf = ptr_to_string(pack.buf);
        self.version = ptr_to_string(pack.version);

        Ok(())
    }
}

impl Serialization for PayloadProtect {
    fn serialize(&self) -> Vec<u8> {
        let pack = PayloadProtectPack {
            channel_id: self.channel_id,
            command_flag: self.command_flag,
            check_sum: self.check_sum,
            v_code: self.v_code,
        };
        let buf = unsafe { SerializePayloadProtect(&pack) };
        buf_to_vec(buf)
    }

    fn parse(&mut self, input: Vec<u8>) -> io::Result<()> {
        let mut pack = PayloadProtectPack::default();

        let buf = SerializedBuffer {
            ptr: input.as_ptr() as *const libc::c_char,
            size: input.len() as u64,
        };
        if unsafe { ParsePayloadProtect(&mut pack, buf) } == 0 {
            return Err(Error::new(
                ErrorKind::Other,
                "cffi ParsePayloadProtect failed",
            ));
        }

        self.channel_id = pack.channel_id;
        self.command_flag = pack.command_flag;
        self.check_sum = pack.check_sum;
        self.v_code = pack.v_code;

        Ok(())
    }
}

impl Serialization for TransferConfig {
    fn serialize(&self) -> Vec<u8> {
        let options = CString::new(self.options.as_str()).unwrap();
        let path = CString::new(self.path.as_str()).unwrap();
        let optional_name = CString::new(self.optional_name.as_str()).unwrap();
        let function_name = CString::new(self.function_name.as_str()).unwrap();
        let client_cwd = CString::new(self.client_cwd.as_str()).unwrap();
        let reserve1 = CString::new(self.reserve1.as_str()).unwrap();
        let reserve2 = CString::new(self.reserve2.as_str()).unwrap();
        let pack = TransferConfigPack {
            file_size: self.file_size,
            atime: self.atime,
            mtime: self.mtime,
            options: options.as_ptr(),
            path: path.as_ptr(),
            optional_name: optional_name.as_ptr(),
            update_if_new: self.update_if_new as u8,
            compress_type: self.compress_type,
            hold_timestamp: self.hold_timestamp as u8,
            function_name: function_name.as_ptr(),
            client_cwd: client_cwd.as_ptr(),
            reserve1: reserve1.as_ptr(),
            reserve2: reserve2.as_ptr(),
        };

        let buf = unsafe { SerializeTransferConfig(&pack) };
        buf_to_vec(buf)
    }

    fn parse(&mut self, input: Vec<u8>) -> io::Result<()> {
        let mut pack = TransferConfigPack {
            file_size: 0,
            atime: 0,
            mtime: 0,
            options: std::ptr::null(),
            path: std::ptr::null(),
            optional_name: std::ptr::null(),
            update_if_new: 0,
            compress_type: 0,
            hold_timestamp: 0,
            function_name: std::ptr::null(),
            client_cwd: std::ptr::null(),
            reserve1: std::ptr::null(),
            reserve2: std::ptr::null(),
        };
        let buf = SerializedBuffer {
            ptr: input.as_ptr() as *const libc::c_char,
            size: input.len() as u64,
        };
        if unsafe { ParseTransferConfig(&mut pack, buf) } == 0 {
            return Err(Error::new(
                ErrorKind::Other,
                "cffi ParseTransferConfig failed",
            ));
        }
        self.file_size = pack.file_size;
        self.atime = pack.atime;
        self.mtime = pack.mtime;
        self.options = ptr_to_string(pack.options);
        self.path = ptr_to_string(pack.path);
        self.optional_name = ptr_to_string(pack.optional_name);
        self.update_if_new = pack.update_if_new > 0;
        self.compress_type = pack.compress_type;
        self.hold_timestamp = pack.hold_timestamp > 0;
        self.function_name = ptr_to_string(pack.function_name);
        self.client_cwd = ptr_to_string(pack.client_cwd);
        self.reserve1 = ptr_to_string(pack.reserve1);
        self.reserve2 = ptr_to_string(pack.reserve2);
        Ok(())
    }
}

impl Serialization for FileMode {
    fn serialize(&self) -> Vec<u8> {
        let context = CString::new(self.context.as_str()).unwrap();
        let full_name = CString::new(self.full_name.as_str()).unwrap();
        let pack = FileModePack {
            perm: self.perm,
            u_id: self.u_id,
            g_id: self.g_id,
            context: context.as_ptr(),
            full_name: full_name.as_ptr(),
        };
        let buf = unsafe { SerializeFileMode(&pack) };
        buf_to_vec(buf)
    }

    fn parse(&mut self, input: Vec<u8>) -> io::Result<()> {
        let mut pack = FileModePack {
            perm: 0,
            u_id: 0,
            g_id: 0,
            context: std::ptr::null(),
            full_name: std::ptr::null(),
        };

        let buf = SerializedBuffer {
            ptr: input.as_ptr() as *const libc::c_char,
            size: input.len() as u64,
        };
        if unsafe { ParseFileMode(&mut pack, buf) } == 0 {
            return Err(Error::new(ErrorKind::Other, "cffi ParseFileMode failed"));
        }

        self.perm = pack.perm;
        self.u_id = pack.u_id;
        self.g_id = pack.g_id;
        self.context = ptr_to_string(pack.context);
        self.full_name = ptr_to_string(pack.full_name);

        Ok(())
    }
}

impl Serialization for TransferPayload {
    fn serialize(&self) -> Vec<u8> {
        let pack = TransferPayloadPack {
            index: self.index,
            compress_type: self.compress_type,
            compress_size: self.compress_size,
            uncompress_size: self.uncompress_size,
        };
        let buf = unsafe { SerializeTransferPayload(&pack) };
        buf_to_vec(buf)
    }

    fn parse(&mut self, input: Vec<u8>) -> io::Result<()> {
        let mut pack = TransferPayloadPack::default();

        let buf = SerializedBuffer {
            ptr: input.as_ptr() as *const libc::c_char,
            size: input.len() as u64,
        };
        if unsafe { ParseTransferPayload(&mut pack, buf) } == 0 {
            return Err(Error::new(
                ErrorKind::Other,
                "cffi ParseTransferPayload failed",
            ));
        }

        self.index = pack.index;
        self.compress_type = pack.compress_type;
        self.compress_size = pack.compress_size;
        self.uncompress_size = pack.uncompress_size;

        Ok(())
    }
}

impl Serialization for PayloadHead {
    fn serialize(&self) -> Vec<u8> {
        let pack = PayloadHeadPack {
            flag: self.flag,
            reserve: self.reserve,
            protocol_ver: self.protocol_ver,
            head_size: self.head_size,
            data_size: self.data_size,
        };
        let buf = unsafe { SerializePayloadHead(&pack) };
        buf_to_vec(buf)
    }

    fn parse(&mut self, input: Vec<u8>) -> io::Result<()> {
        let mut pack = PayloadHeadPack::default();
        let buf = SerializedBuffer {
            ptr: input.as_ptr() as *const libc::c_char,
            size: input.len() as u64,
        };
        if unsafe { ParsePayloadHead(&mut pack, buf) == 0 } {
            return Err(Error::new(ErrorKind::Other, "cffi ParsePayloadHead failed"));
        }

        self.flag = pack.flag;
        self.reserve = pack.reserve;
        self.protocol_ver = pack.protocol_ver;
        self.head_size = pack.head_size;
        self.data_size = pack.data_size;

        Ok(())
    }
}

impl Serialization for UsbHead {
    fn serialize(&self) -> Vec<u8> {
        let pack = UsbHeadPack {
            flag: self.flag,
            option: self.option,
            session_id: self.session_id,
            data_size: self.data_size,
        };
        let buf = unsafe { SerializeUsbHead(&pack) };
        buf_to_vec(buf)
    }

    fn parse(&mut self, input: Vec<u8>) -> io::Result<()> {
        let mut pack = UsbHeadPack::default();
        let buf = SerializedBuffer {
            ptr: input.as_ptr() as *const libc::c_char,
            size: input.len() as u64,
        };
        if unsafe { ParseUsbHead(&mut pack, buf) == 0 } {
            return Err(Error::new(ErrorKind::Other, "cffi ParseUsbHead failed"));
        }

        self.flag = pack.flag;
        self.option = pack.option;
        self.session_id = pack.session_id;
        self.data_size = pack.data_size;

        Ok(())
    }
}

fn ptr_to_string(p: *const libc::c_char) -> String {
    let c_str: &CStr = unsafe { CStr::from_ptr(p) };
    c_str.to_str().unwrap().to_string()
}

pub fn buf_to_vec(buf: SerializedBuffer) -> Vec<u8> {
    let slice =
        unsafe { std::slice::from_raw_parts(buf.ptr as *const libc::c_uchar, buf.size as usize) };
    slice.to_vec()
}
