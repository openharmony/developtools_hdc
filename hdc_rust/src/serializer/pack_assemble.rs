//! packet_assemble
#![allow(missing_docs)]

use crate::common::hsession::TaskMessage;
use crate::config::*;
use crate::serializer::native_struct;
use crate::serializer::serialize::Serialization;
#[allow(unused)]
use crate::utils::hdc_log::*;

use std::io::{self, Error, ErrorKind};

fn calc_check_sum(data: &[u8]) -> u8 {
    data.iter().sum()
}

pub fn unpack_payload_head(data: Vec<u8>) -> io::Result<native_struct::PayloadHead> {
    if data[..PACKET_FLAG.len()] != PACKET_FLAG[..] {
        return Err(Error::new(
            ErrorKind::Other,
            format!("PACKET_FLAG incorrect, content: {:#?}", data),
        ));
    }

    let mut payload_head = native_struct::PayloadHead::default();
    payload_head.parse(data)?;
    Ok(payload_head)
}

pub fn unpack_payload_protect(data: Vec<u8>) -> io::Result<native_struct::PayloadProtect> {
    let mut payload_protect = native_struct::PayloadProtect::default();
    payload_protect.parse(data)?;
    if payload_protect.v_code != PAYLOAD_VCODE {
        return Err(Error::new(
            ErrorKind::Other,
            "Session recv static vcode failed",
        ));
    }
    Ok(payload_protect)
}

pub fn concat_pack(task_message: TaskMessage) -> Vec<u8> {
    // let data = obj.serialize();
    let check_sum: u8 = if ENABLE_IO_CHECK {
        calc_check_sum(&task_message.payload)
    } else {
        0
    };
    let payload_protect = native_struct::PayloadProtect {
        channel_id: task_message.channel_id,
        command_flag: task_message.command as u32,
        check_sum,
        v_code: PAYLOAD_VCODE,
    };

    let protect_buf = payload_protect.serialize();

    let payload_head = native_struct::PayloadHead {
        flag: [PACKET_FLAG[0], PACKET_FLAG[1]],
        protocol_ver: VER_PROTOCOL as u8,
        head_size: (protect_buf.len() as u16).to_be(),
        data_size: (task_message.payload.len() as u32).to_be(),
        reserve: [0, 0],
    };

    crate::trace!(
        "concat pack, datasize: {}, slicelen: {}, headsize: {}",
        task_message.payload.len(),
        task_message.payload.as_slice().len(),
        protect_buf.len()
    );

    let head_buf = payload_head.serialize();
    [
        head_buf.as_slice(),
        protect_buf.as_slice(),
        task_message.payload.as_slice(),
    ]
    .concat()
}
