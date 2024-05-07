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
//! auth
#![allow(missing_docs)]

use crate::config::*;

use hdc::config;
use hdc::utils;
use hdc::config::TaskMessage;
use hdc::host_transfer::host_usb;
use hdc::serializer::serialize::Serialization;
use hdc::serializer::native_struct::SessionHandShake;
use hdc::transfer;

use std::io::{self, Error, ErrorKind};
use std::path::Path;

use openssl::base64;
use openssl::rsa::{Padding, Rsa};
#[cfg(feature = "host")]
extern crate ylong_runtime_static as ylong_runtime;
use ylong_runtime::net::SplitReadHalf;

pub async fn usb_handshake_with_daemon(
    ptr: u64,
    connect_key: String,
    session_id: u32,
    channel_id: u32,
) -> io::Result<(String, String)> {
    let rsa = load_or_create_prikey()?;

    let mut handshake = SessionHandShake {
        banner: HANDSHAKE_MESSAGE.to_string(),
        session_id,
        connect_key: connect_key.clone(),
        version: config::get_version(),
        ..Default::default()
    };

    send_handshake_to_daemon(&handshake, channel_id).await;
    loop {
        let mut rx = host_usb::start_recv_once(ptr, connect_key.clone(), session_id);
        let (msg, _package_index) = match rx.recv().await {
            Ok((msg, index)) => (msg, index),
            Err(_) => {
                println!("usb handshake recv fail");
                return Err(utils::error_other("usb recv failed, reopen...".to_string()));
            }
        };
        if msg.command == config::HdcCommand::KernelHandshake {
            let mut recv = SessionHandShake::default();
            recv.parse(msg.payload)?;

            hdc::info!("recv handshake: {:#?}", recv);
            if recv.banner != config::HANDSHAKE_MESSAGE {
                return Err(Error::new(ErrorKind::Other, "Recv server-hello failed"));
            }

            if recv.auth_type == config::AuthType::OK as u8 {
                return Ok((recv.buf, recv.version));
            } else if recv.auth_type == config::AuthType::Publickey as u8 {
                // send public key
                handshake.auth_type = config::AuthType::Publickey as u8;
                handshake.buf = get_hostname()?;
                handshake.buf.push(char::from_u32(12).unwrap());
                let pubkey_pem = get_pubkey_pem(&rsa)?;
                handshake.buf.push_str(pubkey_pem.as_str());
                send_handshake_to_daemon(&handshake, channel_id).await;

                // send signature
                handshake.auth_type = config::AuthType::Signature as u8;
                handshake.buf = get_signature_b64(&rsa, recv.buf)?;
                send_handshake_to_daemon(&handshake, channel_id).await;
            } else if recv.auth_type == config::AuthType::Fail as u8 {
                return Err(Error::new(ErrorKind::Other, recv.buf.as_str()));
            } else {
                return Err(Error::new(ErrorKind::Other, "unknown auth type"));
            }
        } else {
            return Err(Error::new(ErrorKind::Other, "unknown command flag"));
        }
    }
}

pub async fn handshake_with_daemon(
    connect_key: String,
    session_id: u32,
    channel_id: u32,
    rd: &mut SplitReadHalf,
) -> io::Result<(String, String)> {
    let rsa = load_or_create_prikey()?;

    let mut handshake = SessionHandShake {
        banner: HANDSHAKE_MESSAGE.to_string(),
        session_id,
        connect_key,
        version: config::get_version(),
        ..Default::default()
    };

    send_handshake_to_daemon(&handshake, channel_id).await;
    loop {
        let msg = transfer::tcp::unpack_task_message(rd).await?;
        if msg.command == config::HdcCommand::KernelHandshake {
            let mut recv = SessionHandShake::default();
            recv.parse(msg.payload)?;

            hdc::info!("recv handshake: {:#?}", recv);
            if recv.banner != config::HANDSHAKE_MESSAGE {
                return Err(Error::new(ErrorKind::Other, "Recv server-hello failed"));
            }

            if recv.auth_type == config::AuthType::OK as u8 {
                return Ok((recv.buf, recv.version));
            } else if recv.auth_type == config::AuthType::Publickey as u8 {
                // send public key
                handshake.auth_type = config::AuthType::Publickey as u8;
                handshake.buf = get_hostname()?;
                handshake.buf.push(char::from_u32(12).unwrap());
                let pubkey_pem = get_pubkey_pem(&rsa)?;
                handshake.buf.push_str(pubkey_pem.as_str());
                send_handshake_to_daemon(&handshake, channel_id).await;

                // send signature
                handshake.auth_type = config::AuthType::Signature as u8;
                handshake.buf = get_signature_b64(&rsa, recv.buf)?;
                send_handshake_to_daemon(&handshake, channel_id).await;
            } else if recv.auth_type == config::AuthType::Fail as u8 {
                return Err(Error::new(ErrorKind::Other, recv.buf.as_str()));
            } else {
                return Err(Error::new(ErrorKind::Other, "unknown auth type"));
            }
        } else {
            return Err(Error::new(ErrorKind::Other, "unknown command flag"));
        }
    }
}

fn load_or_create_prikey() -> io::Result<Rsa<openssl::pkey::Private>> {
    let file = Path::new(&get_home_dir())
        .join(config::RSA_PRIKEY_PATH)
        .join(config::RSA_PRIKEY_NAME);

    if let Ok(pem) = std::fs::read(&file) {
        if let Ok(prikey) = Rsa::private_key_from_pem(&pem) {
            hdc::info!("found existed private key");
            return Ok(prikey);
        } else {
            hdc::error!("found broken private key, regenerating...");
        }
    }

    hdc::info!("create private key at {:#?}", file);
    create_prikey()
}

pub fn create_prikey() -> io::Result<Rsa<openssl::pkey::Private>> {
    let prikey = Rsa::generate(config::RSA_BIT_NUM as u32).unwrap();
    let pem = prikey.private_key_to_pem().unwrap();
    let path = Path::new(&get_home_dir()).join(config::RSA_PRIKEY_PATH);
    let file = path.join(config::RSA_PRIKEY_NAME);

    let _ = std::fs::create_dir_all(&path);

    if std::fs::write(file, pem).is_err() {
        hdc::error!("write private key failed");
        Err(Error::new(ErrorKind::Other, "write private key failed"))
    } else {
        Ok(prikey)
    }
}

fn get_pubkey_pem(rsa: &Rsa<openssl::pkey::Private>) -> io::Result<String> {
    if let Ok(pubkey) = rsa.public_key_to_pem() {
        if let Ok(buf) = String::from_utf8(pubkey) {
            Ok(buf)
        } else {
            Err(Error::new(
                ErrorKind::Other,
                "convert public key to pem string failed",
            ))
        }
    } else {
        Err(Error::new(
            ErrorKind::Other,
            "convert public key to pem string failed",
        ))
    }
}

fn get_signature_b64(rsa: &Rsa<openssl::pkey::Private>, plain: String) -> io::Result<String> {
    let mut enc = vec![0_u8; config::RSA_BIT_NUM];
    match rsa.private_encrypt(plain.as_bytes(), &mut enc, Padding::PKCS1) {
        Ok(size) => Ok(base64::encode_block(&enc[..size])),
        Err(_) => Err(Error::new(ErrorKind::Other, "rsa private encrypt failed")),
    }
}

async fn send_handshake_to_daemon(handshake: &SessionHandShake, channel_id: u32) {
    transfer::put(
        handshake.session_id,
        TaskMessage {
            channel_id,
            command: config::HdcCommand::KernelHandshake,
            payload: handshake.serialize(),
        },
    )
    .await;
}

fn get_home_dir() -> String {
    use std::process::Command;

    let output = if cfg!(target_os = "windows") {
        Command::new("cmd")
            .args(["/c", "echo %USERPROFILE%"])
            .output()
    } else {
        Command::new("sh").args(["-c", "echo ~"]).output()
    };

    if let Ok(result) = output {
        String::from_utf8(result.stdout).unwrap().trim().to_string()
    } else {
        hdc::warn!("get home dir failed, use current dir instead");
        ".".to_string()
    }
}

fn get_hostname() -> io::Result<String> {

    use std::process::Command;
    
    let output = if cfg!(target_os = "windows") {
        Command::new("cmd").args(["/c", "hostname"]).output()
    } else {
        Command::new("cmd").args(["-c", "hostname"]).output()
    };
   
    if let Ok(result) = output {
        Ok(String::from_utf8(result.stdout).unwrap())
    } else {
        Err(Error::new(ErrorKind::Other, "get hostname failed"))
    }
}
