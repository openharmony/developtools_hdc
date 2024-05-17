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
//! packet_assemble
#![allow(missing_docs)]

use crate::config::TaskMessage;
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
            format!(
                "PACKET_FLAG incorrect, content: {:?}",
                data.iter()
                    .map(|&c| format!("{c:02X}"))
                    .collect::<Vec<_>>()
                    .join(" ")
            ),
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

    let head_buf = payload_head.serialize();
    [
        head_buf.as_slice(),
        protect_buf.as_slice(),
        task_message.payload.as_slice(),
    ]
    .concat()
}
