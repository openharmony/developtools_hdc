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
//! tcp
#![allow(missing_docs)]

use crate::config::*;
use crate::serializer;
#[allow(unused)]
use crate::utils::hdc_log::*;

use std::io::{self, Error, ErrorKind};

#[cfg(feature = "host")]
extern crate ylong_runtime_static as ylong_runtime;
use ylong_runtime::io::AsyncReadExt;
use ylong_runtime::net::SplitReadHalf;

async fn read_frame(rd: &mut SplitReadHalf, expected_size: usize) -> io::Result<Vec<u8>> {
    if expected_size == 0 {
        return Ok(vec![]);
    }
    let mut data = vec![0_u8; expected_size];
    let mut index: usize = 0;
    while index < expected_size {
        crate::trace!("before read {index} / {expected_size}");
        match rd.read(&mut data[index..]).await {
            Ok(recv_size) => {
                crate::trace!("after read {recv_size}");
                if recv_size == 0 {
                    crate::debug!("peer shutdown");
                    return Err(Error::new(ErrorKind::ConnectionAborted, "peer shutdown"));
                }
                index += recv_size;
            }
            Err(e) => {
                crate::warn!("read tcp failed: {}", e.to_string());
                return Err(Error::new(ErrorKind::Other, "read tcp failed"));
            }
        }
    }
    Ok(data)
}

pub async fn unpack_task_message(rd: &mut SplitReadHalf) -> io::Result<TaskMessage> {
    let data = read_frame(rd, serializer::HEAD_SIZE).await?;
    let payload_head = serializer::unpack_payload_head(data)?;
    crate::trace!("get payload_head: {:?}", payload_head);

    let expected_head_size = u16::from_be(payload_head.head_size) as usize;
    let expected_data_size = u32::from_be(payload_head.data_size) as usize;
    if expected_head_size + expected_data_size == 0
        || expected_head_size + expected_data_size > HDC_BUF_MAX_SIZE
    {
        return Err(Error::new(ErrorKind::Other, "Packet size incorrect"));
    }

    let data = read_frame(rd, expected_head_size).await?;
    let payload_protect = serializer::unpack_payload_protect(data)?;
    crate::trace!("get payload_protect: {:?}", payload_protect);
    let channel_id = payload_protect.channel_id;

    let command = match HdcCommand::try_from(payload_protect.command_flag) {
        Ok(command) => command,
        Err(_) => {
            return Err(Error::new(ErrorKind::Other, "unknown command"));
        }
    };

    let payload = read_frame(rd, expected_data_size).await?;
    Ok(TaskMessage {
        channel_id,
        command,
        payload,
    })
}

pub async fn recv_channel_message(rd: &mut SplitReadHalf) -> io::Result<Vec<u8>> {
    let data = read_frame(rd, 4).await?;
    let Ok(data) = data.try_into() else {
        return Err(Error::new(
            ErrorKind::Other,
            "Data forced conversion failed",
        ));
    };
    let expected_size = u32::from_be_bytes(data);
    read_frame(rd, expected_size as usize).await
}
