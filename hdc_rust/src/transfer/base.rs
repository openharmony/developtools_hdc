//! base
#![allow(missing_docs)]

use ylong_runtime::sync::mpsc::BoundedSender;

use crate::common::hsession::TaskMessage;
use crate::config::*;
use crate::serializer;
use crate::utils;
#[allow(unused)]
use crate::utils::hdc_log::*;

use std::io::{self, Error, ErrorKind};

pub trait Writer {
    fn write_all(&self, data: Vec<u8>) -> io::Result<()>;
}

pub trait Reader: Send + Sync + 'static {
    fn read_frame(&self, expected_size: usize) -> io::Result<Vec<u8>>;
    fn check_protocol_head(&self) -> io::Result<u32> {
        Err(utils::error_other("not implemeted".to_string()))
    }
}

pub fn unpack_task_message(rd: &dyn Reader, tx: BoundedSender<TaskMessage>) -> io::Result<()> {
    let pack_size = rd.check_protocol_head()?;
    if pack_size == 0 {
        return Err(Error::new(ErrorKind::WriteZero, "dummy package"));
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

        let (protect, payload) = body.split_at(expected_head_size);

        let payload_protect = serializer::unpack_payload_protect(protect.to_vec())?;
        let channel_id = payload_protect.channel_id;

        let command = match HdcCommand::try_from(payload_protect.command_flag) {
            Ok(command) => command,
            Err(_) => {
                return Err(Error::new(ErrorKind::Other, "unknown command"));
            }
        };

        let _ = tx
            .send(TaskMessage {
                channel_id,
                command,
                payload: payload.to_vec(),
            })
            .await;
        Ok(())
    });

    Ok(())
}
