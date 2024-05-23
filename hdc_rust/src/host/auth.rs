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
use hdc::config::TaskMessage;
use hdc::serializer::native_struct::{SessionHandShake};
use hdc::serializer::serialize::Serialization;
use hdc::transfer;

use std::io::{self, Error, ErrorKind};
use std::path::Path;

use openssl::base64;
use openssl::rsa::{Padding, Rsa};
#[cfg(feature = "host")]
extern crate ylong_runtime_static as ylong_runtime;
use hdc::common::base::Base;
use crate::task::{ConnectMap, ConnectStatus, DaemonInfo};

pub async fn start_handshake_with_daemon(
    connect_key: String,
    session_id: u32,
    channel_id: u32,
    conn_type: ConnectType,
) {
    let handshake = SessionHandShake {
        banner: HANDSHAKE_MESSAGE.to_string(),
        session_id,
        connect_key: connect_key.clone(),
        version: config::get_version(),
        auth_type: config::AuthType::None as u8,
        ..Default::default()
    };

    send_handshake_to_daemon(&handshake, channel_id).await;

    ConnectMap::put(
        connect_key.clone(),
        DaemonInfo {
            session_id,
            conn_type,
            version: "unknown".to_string(),
            conn_status: ConnectStatus::Unknown,
            dev_name: "unknown".to_string(),
            emg_msg: "".to_string(),
            daemon_auth_status: DAEOMN_UNAUTHORIZED.to_string(),
        },
    ).await;
}

async fn handshake_deal_daemon_auth_result(daemon: SessionHandShake, connect_key: String) -> io::Result<()> {
    let mut devname = "".to_string();
    let mut auth_result = "".to_string();
    let mut emg_msg = "".to_string();

    if daemon.version.as_str() < "Ver: 3.0.0b" {
        if !daemon.buf.is_empty() {
            devname = daemon.buf;
        }
    } else {
        let auth_info = match Base::tlv_to_stringmap(daemon.buf.as_str()) {
            Some(tlv_map) => tlv_map,
            _ => { return Err(Error::new(ErrorKind::Other, "parse tlv failed")); },
        };
        devname = match auth_info.get(TAG_DEVNAME) {
            Some(devname) => devname.to_string(),
            _ => "".to_string(),
        };
        auth_result = match auth_info.get(TAG_DAEOMN_AUTHSTATUS) {
            Some(auth_result) => auth_result.to_string(),
            _ => "".to_string(),
        };
        emg_msg = match auth_info.get(TAG_EMGMSG) {
            Some(emg_msg) => emg_msg.to_string(),
            _ => "".to_string(),
        };
    }

    hdc::info!("daemon auth result[{}] key[{}] ver[{}] devname[{}] emgmsg[{}]",
            auth_result.clone(), connect_key.clone(), daemon.version.clone(),
            devname.clone(), emg_msg.clone());

    if ConnectMap::update(
        connect_key.clone(),
        ConnectStatus::Connected,
        daemon.version.to_string(),
        devname.to_string(),
        emg_msg.to_string(),
        auth_result.to_string()
    ).await {
        Ok(())
    } else {
        hdc::error!("update connect status for {} failed", connect_key);
        Err(Error::new(ErrorKind::Other, "not exist connect key"))
    }
}

pub async fn handshake_task(msg: TaskMessage, session_id: u32, connect_key: String) -> io::Result<()> {
    let rsa = load_or_create_prikey()?;
    let mut recv = SessionHandShake::default();
    let channel_id = msg.channel_id;
    recv.parse(msg.payload)?;
    hdc::info!("recv handshake: {:#?}", recv);

    if recv.banner != config::HANDSHAKE_MESSAGE {
        hdc::info!("invalid banner {}", recv.banner);
        return Err(Error::new(ErrorKind::Other, "Recv server-hello failed"));
    }

    if recv.auth_type == config::AuthType::OK as u8 {
        handshake_deal_daemon_auth_result(recv.clone(), connect_key.clone()).await
    } else if recv.auth_type == config::AuthType::Publickey as u8 {
        // send public key
        let pubkey_pem = get_pubkey_pem(&rsa)?;
        let mut buf = get_hostname()?;
        buf.push(char::from_u32(12).unwrap());
        buf.push_str(pubkey_pem.as_str());
        let handshake = SessionHandShake {
            banner: HANDSHAKE_MESSAGE.to_string(),
            session_id,
            connect_key: connect_key.clone(),
            version: config::get_version(),
            auth_type: config::AuthType::Publickey as u8,
            buf,
        };
        send_handshake_to_daemon(&handshake, channel_id).await;
        return Ok(());
    } else if recv.auth_type == config::AuthType::Signature as u8 {
        // send signature
        let buf = get_signature_b64(&rsa, recv.buf)?;
        let handshake = SessionHandShake {
            banner: HANDSHAKE_MESSAGE.to_string(),
            session_id,
            connect_key: connect_key.clone(),
            version: config::get_version(),
            auth_type: config::AuthType::Signature as u8,
            buf,
        };
        send_handshake_to_daemon(&handshake, channel_id).await;
        return Ok(());
    } else if recv.auth_type == config::AuthType::Fail as u8 {
        hdc::info!("daemon auth failed");
        return Err(Error::new(ErrorKind::Other, recv.buf.as_str()));
    } else {
        hdc::info!("invalid auth type {}", recv.auth_type);
        return Err(Error::new(ErrorKind::Other, "unknown auth type"));
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
    hdc::info!("send handshake: {:#?}", handshake.clone());
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
