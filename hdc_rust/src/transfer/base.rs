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
//! base
#![allow(missing_docs)]

#[cfg(feature = "host")]
extern crate ylong_runtime_static as ylong_runtime;
use ylong_runtime::sync::mpsc::BoundedSender;

use crate::config::TaskMessage;
use crate::config::*;
use crate::serializer;
use crate::utils;
#[allow(unused)]
use crate::utils::hdc_log::*;

use std::io::{self, Error, ErrorKind};
use std::sync::Arc;
use ylong_runtime::sync::Mutex;

type BOOL_ = Arc<Mutex<bool>>;

pub struct CheckCompressVersion {}
impl CheckCompressVersion {
    pub fn get_instance() -> BOOL_ {
        static mut CAN_COMPRESS: Option<BOOL_> = Option::None;
        unsafe {
            CAN_COMPRESS
                .get_or_insert_with(|| Arc::new(Mutex::new(false)))
                .clone()
        }
    }

    pub async fn set(check_version: bool) {
        let arc = Self::get_instance();
        let mut mutex = arc.lock().await;
        *mutex = check_version;
    }

    pub async fn get() -> bool {
        let arc = Self::get_instance();
        let mutex = arc.lock().await;
        *mutex
    }
}

pub trait Writer {
    fn write_all(&self, data: Vec<u8>) -> io::Result<i32>;
}

pub trait Reader: Send + Sync + 'static {
    fn read_frame(&self, expected_size: usize) -> io::Result<Vec<u8>>;
    fn check_protocol_head(&mut self) -> io::Result<(u32, u32)> {
        Err(utils::error_other("not implemeted".to_string()))
    }
    fn process_head(&self) {}
}

pub async fn unpack_task_message_lock(
    rd: &mut dyn Reader,
    pack_size: u32,
    tx: BoundedSender<TaskMessage>,
) -> io::Result<()> {
    let data = rd.read_frame(pack_size as usize)?;
    let (head, body) = data.split_at(serializer::HEAD_SIZE);
    let payload_head = serializer::unpack_payload_head(head.to_vec());
    match payload_head {
        Ok(payload_head) => {
            let expected_head_size = u16::from_be(payload_head.head_size) as usize;
            let expected_data_size = u32::from_be(payload_head.data_size) as usize;

            if serializer::HEAD_SIZE + expected_head_size + expected_data_size != pack_size as usize
            {
                crate::warn!(
            "protocol size diff: {pack_size} != {} + {expected_head_size} + {expected_data_size}",
            serializer::HEAD_SIZE
        );
            }

            if expected_head_size + expected_data_size == 0
                || expected_head_size + expected_data_size > HDC_BUF_MAX_SIZE
            {
                return Err(Error::new(ErrorKind::Other, "Packet size incorrect"));
            }

            let protect = body.split_at(expected_head_size).0;
            let payload = match expected_head_size + expected_data_size <= body.len() {
                true => body.split_at(expected_head_size).1,
                false => {
                    body.split_at(expected_head_size)
                        .1
                        .split_at(expected_data_size)
                        .0
                }
            };

            let payload_protect = serializer::unpack_payload_protect(protect.to_vec())?;
            let channel_id = payload_protect.channel_id;

            let command = match HdcCommand::try_from(payload_protect.command_flag) {
                Ok(command) => command,
                Err(_) => {
                    return Err(Error::new(ErrorKind::Other, "unknown command"));
                }
            };
            let mut remaining = (expected_data_size - payload.len()) as i32;
            if remaining == 0 {
                let _ = tx
                    .send(TaskMessage {
                        channel_id,
                        command,
                        payload: payload.to_vec(),
                    })
                    .await;
            }
            let mut total_payload = payload.to_vec();
            while remaining > 0 {
                let head_result = rd.check_protocol_head();
                match head_result {
                    Ok((packet_size, _pkg_index)) => {
                        rd.process_head();
                        if packet_size == 0 {
                            continue;
                        }
                        let mut payload1 = rd.read_frame(packet_size as usize).unwrap();
                        total_payload.append(&mut payload1);
                        remaining -= packet_size as i32;
                        println!("remaining:{}, packet_size:{}", remaining, packet_size);
                        if remaining == 0 {
                            let _ = tx
                                .send(TaskMessage {
                                    channel_id,
                                    command,
                                    payload: total_payload,
                                })
                                .await;
                            break;
                        }
                    }
                    Err(e) => {
                        println!("check head error: {:#?}", e);
                        return Err(e);
                    }
                }
            }

            let _ = tx
                .send(TaskMessage {
                    channel_id,
                    command: HdcCommand::UartFinish,
                    payload: vec![],
                })
                .await;
            Ok(())
        }
        Err(e) => {
            println!("uart unpack_task_message_lock, err:{:#?}", e);
            Err(e)
        }
    }
}

pub fn unpack_task_message(
    rd: &mut dyn Reader,
    tx: BoundedSender<(TaskMessage, u32)>,
) -> io::Result<()> {
    let (pack_size, package_index) = rd.check_protocol_head()?;
    if pack_size == 0 {
        return Ok(());
    }

    let data = rd.read_frame(pack_size as usize)?;
    ylong_runtime::spawn(async move {
        let (head, body) = data.split_at(serializer::HEAD_SIZE);
        let payload_head = serializer::unpack_payload_head(head.to_vec())?;
        let expected_head_size = u16::from_be(payload_head.head_size) as usize;
        let expected_data_size = u32::from_be(payload_head.data_size) as usize;

        if serializer::HEAD_SIZE + expected_head_size + expected_data_size != pack_size as usize {
            crate::warn!(
                "protocol size diff: {pack_size} != {} + {expected_head_size} + {expected_data_size}",
                serializer::HEAD_SIZE
            );
        }

        if expected_head_size + expected_data_size == 0
            || expected_head_size + expected_data_size > HDC_BUF_MAX_SIZE
        {
            return Err(Error::new(ErrorKind::Other, "Packet size incorrect"));
        }

        let protect = body.split_at(expected_head_size).0;
        let payload = match expected_head_size + expected_data_size <= body.len() {
            true => body.split_at(expected_head_size).1,
            false => {
                body.split_at(expected_head_size)
                    .1
                    .split_at(expected_data_size)
                    .0
            }
        };

        let payload_protect = serializer::unpack_payload_protect(protect.to_vec())?;
        let channel_id = payload_protect.channel_id;

        let command = match HdcCommand::try_from(payload_protect.command_flag) {
            Ok(command) => command,
            Err(_) => {
                return Err(Error::new(ErrorKind::Other, "unknown command"));
            }
        };

        let _ = tx
            .send((
                TaskMessage {
                    channel_id,
                    command,
                    payload: payload.to_vec(),
                },
                package_index,
            ))
            .await;
        Ok(())
    });

    Ok(())
}
