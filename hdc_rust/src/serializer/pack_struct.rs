//! pack_struct
#![allow(missing_docs)]

extern crate libc;

use std::mem;

#[repr(C)]
pub struct SessionHandShakePack {
    pub banner: *const libc::c_char,
    pub auth_type: libc::c_uchar,
    pub session_id: libc::c_uint,
    pub connect_key: *const libc::c_char,
    pub buf: *const libc::c_char,
    pub version: *const libc::c_char,
}

#[repr(C)]
#[derive(Default)]
pub struct PayloadProtectPack {
    pub channel_id: libc::c_uint,
    pub command_flag: libc::c_uint,
    pub check_sum: libc::c_uchar,
    pub v_code: libc::c_uchar,
}

#[repr(C)]
pub struct TransferConfigPack {
    pub file_size: libc::c_ulonglong,
    pub atime: libc::c_ulonglong,
    pub mtime: libc::c_ulonglong,
    pub options: *const libc::c_char,
    pub path: *const libc::c_char,
    pub optional_name: *const libc::c_char,
    pub update_if_new: libc::c_uchar,
    pub compress_type: libc::c_uchar,
    pub hold_timestamp: libc::c_uchar,
    pub function_name: *const libc::c_char,
    pub client_cwd: *const libc::c_char,
    pub reserve1: *const libc::c_char,
    pub reserve2: *const libc::c_char,
}

#[repr(C)]
pub struct FileModePack {
    pub perm: libc::c_ulonglong,
    pub u_id: libc::c_ulonglong,
    pub g_id: libc::c_ulonglong,
    pub context: *const libc::c_char,
    pub full_name: *const libc::c_char,
}

#[repr(C)]
#[derive(Default)]
pub struct TransferPayloadPack {
    pub index: libc::c_ulonglong,
    pub compress_type: libc::c_uchar,
    pub compress_size: libc::c_uint,
    pub uncompress_size: libc::c_uint,
}

#[repr(C, packed)]
#[derive(Default)]
pub struct PayloadHeadPack {
    pub flag: [libc::c_uchar; 2],
    pub reserve: [libc::c_uchar; 2],
    pub protocol_ver: libc::c_uchar,
    pub head_size: libc::c_ushort,
    pub data_size: libc::c_uint,
}

#[allow(unused)]
#[repr(C, packed)]
#[derive(Default)]
pub struct UartHeadPack {
    pub flag: [libc::c_uchar; 2],
    pub option: libc::c_ushort,
    pub session_id: libc::c_uint,
    pub data_size: libc::c_uint,
    pub package_index: libc::c_uint,
    pub data_checksum: libc::c_uint,
    pub head_checksum: libc::c_uint,
}

#[allow(unused)]
#[repr(C, packed)]
#[derive(Default)]
pub struct UsbHeadPack {
    pub flag: [libc::c_uchar; 2],
    pub option: libc::c_uchar,
    pub session_id: libc::c_uint,
    pub data_size: libc::c_uint,
}

#[allow(unused)]
#[repr(C, packed)]
pub struct ChannelHandShakePack {
    pub banner: [libc::c_char; 12],
    pub version: [libc::c_char; BUF_SIZE_TINY as usize],
}

#[allow(unused)]
const MAX_CONNECTKEY_SIZE: u16 = 32;
#[allow(unused)]
const BUF_SIZE_TINY: u16 = 64;

pub const HEAD_SIZE: usize = mem::size_of::<PayloadHeadPack>();
pub const USB_HEAD_SIZE: usize = mem::size_of::<UsbHeadPack>();
pub const UART_HEAD_SIZE: usize = mem::size_of::<UartHeadPack>();
