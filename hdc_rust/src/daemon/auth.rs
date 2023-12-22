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

use hdc::config::{self, *};
use hdc::serializer::native_struct;
use hdc::serializer::serialize::Serialization;
use hdc::transfer;

use crate::transfer::base::CheckCompressVersion;
use openssl::base64;
use openssl::rsa::{Padding, Rsa};
use ylong_runtime::sync::RwLock;

use crate::utils::hdc_log::*;
use std::collections::HashMap;
use std::io::Write;
use std::io::{self, Error, ErrorKind};
use std::path::Path;
use std::sync::Arc;

#[derive(Clone, PartialEq, Eq)]
pub enum AuthStatus {
    Init(String),           // with plain
    Pubk((String, String)), // with (plain, pk)
    Ok,
    Fail,
}

type AuthStatusMap_ = Arc<RwLock<HashMap<u32, AuthStatus>>>;

pub struct AuthStatusMap {}
impl AuthStatusMap {
    fn get_instance() -> AuthStatusMap_ {
        static mut AUTH_STATUS_MAP: Option<AuthStatusMap_> = None;
        unsafe {
            AUTH_STATUS_MAP
                .get_or_insert_with(|| Arc::new(RwLock::new(HashMap::new())))
                .clone()
        }
    }

    pub async fn get(session_id: u32) -> AuthStatus {
        let instance = Self::get_instance();
        let map = instance.read().await;
        map.get(&session_id).unwrap().clone()
    }

    async fn put(session_id: u32, auth_status: AuthStatus) {
        let instance = Self::get_instance();
        let mut map = instance.write().await;
        map.insert(session_id, auth_status);
    }
}

pub async fn handshake_init(task_message: TaskMessage) -> io::Result<(u32, TaskMessage)> {
    if task_message.command != HdcCommand::KernelHandshake {
        return Err(Error::new(ErrorKind::Other, "unknown command flag"));
    }

    let mut recv = native_struct::SessionHandShake::default();
    recv.parse(task_message.payload)?;

    hdc::info!("recv handshake: {:#?}", recv);
    if recv.banner != HANDSHAKE_MESSAGE {
        return Err(Error::new(ErrorKind::Other, "Recv server-hello failed"));
    }

    // auth is not required
    if recv.version.as_str() < "Ver: 1.3.1" {
        CheckCompressVersion::set(false).await;
        return Ok((
            recv.session_id,
            make_ok_message(recv.session_id, task_message.channel_id).await,
        ));
    }

    // auth is required
    let buf = hdc::utils::get_current_time().to_string();
    AuthStatusMap::put(recv.session_id, AuthStatus::Init(buf.clone())).await;

    let send = native_struct::SessionHandShake {
        banner: HANDSHAKE_MESSAGE.to_string(),
        session_id: 0,
        connect_key: "".to_string(),
        buf,
        auth_type: AuthType::Token as u8,
        version: get_version(),
    };

    hdc::info!("send handshake: {:#?}", send);
    let message = TaskMessage {
        channel_id: task_message.channel_id,
        command: HdcCommand::KernelHandshake,
        payload: send.serialize(),
    };
    Ok((recv.session_id, message))
}

async fn make_ok_message(session_id: u32, channel_id: u32) -> TaskMessage {
    AuthStatusMap::put(session_id, AuthStatus::Ok).await;

    let send = native_struct::SessionHandShake {
        banner: HANDSHAKE_MESSAGE.to_string(),
        session_id: 0,
        connect_key: "".to_string(),
        auth_type: AuthType::OK as u8,
        version: get_version(),
        buf: match nix::unistd::gethostname() {
            Ok(hostname) => hostname.into_string().unwrap(),
            Err(_) => String::from("unknown"),
        },
    };
    TaskMessage {
        channel_id,
        command: HdcCommand::KernelHandshake,
        payload: send.serialize(),
    }
}

pub async fn handshake_task(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    let mut recv = native_struct::SessionHandShake::default();
    recv.parse(task_message.payload)?;

    let channel_id = task_message.channel_id;

    if recv.auth_type == AuthType::Publickey as u8 {
        let plain = if let AuthStatus::Init(buf) = AuthStatusMap::get(session_id).await {
            buf
        } else {
            handshake_fail(session_id, channel_id, "auth failed".to_string()).await;
            return Ok(());
        };

        let known_hosts = read_known_hosts_pubkey();
        let pubkey = recv.buf.trim().to_string();

        if !known_hosts.contains(&pubkey) {
            hdc::debug!("get new pubkey: {}", &pubkey);
            if !require_user_permittion(&pubkey) {
                handshake_fail(
                    session_id,
                    channel_id,
                    "public key refused by device".to_string(),
                )
                .await;
                return Ok(());
            }
            if write_known_hosts_pubkey(&pubkey).is_err() {
                handshake_fail(
                    session_id,
                    channel_id,
                    "write public key failed".to_string(),
                )
                .await;
                return Ok(());
            }
        } else {
            hdc::info!("pubkey matches known host");
        }

        AuthStatusMap::put(session_id, AuthStatus::Pubk((plain, pubkey))).await;
    } else if recv.auth_type == AuthType::Signature as u8 {
        match validate_signature(recv.buf, session_id).await {
            Ok(()) => {
                transfer::put(session_id, make_ok_message(session_id, channel_id).await).await;
                transfer::put(
                    session_id,
                    TaskMessage {
                        channel_id,
                        command: HdcCommand::KernelChannelClose,
                        payload: vec![0],
                    },
                )
                .await;
                AuthStatusMap::put(session_id, AuthStatus::Ok).await;
            }
            Err(e) => {
                let errlog = e.to_string();
                hdc::error!("validate signature failed: {}", &errlog);
                handshake_fail(session_id, channel_id, errlog).await;
            }
        }
    } else {
        transfer::put(session_id, make_ok_message(session_id, channel_id).await).await;
    }
    Ok(())
}

async fn validate_signature(signature: String, session_id: u32) -> io::Result<()> {
    let (plain, pubkey) =
        if let AuthStatus::Pubk((plain, pubkey)) = AuthStatusMap::get(session_id).await {
            (plain, pubkey)
        } else {
            return Err(Error::new(ErrorKind::Other, "auth failed"));
        };

    let signature_bytes = if let Ok(bytes) = base64::decode_block(&signature) {
        bytes
    } else {
        return Err(Error::new(ErrorKind::Other, "signature decode failed"));
    };

    let rsa = if let Ok(cipher) = Rsa::public_key_from_pem(pubkey.as_bytes()) {
        cipher
    } else {
        return Err(Error::new(ErrorKind::Other, "pubkey convert failed"));
    };

    let mut buf = vec![0_u8; config::RSA_BIT_NUM];
    let dec_size = rsa
        .public_decrypt(&signature_bytes, &mut buf, Padding::PKCS1)
        .unwrap_or(0);

    if plain.as_bytes() == &buf[..dec_size] {
        Ok(())
    } else {
        Err(Error::new(ErrorKind::Other, "signature not match"))
    }
}

fn read_known_hosts_pubkey() -> Vec<String> {
    let file_name = Path::new(config::RSA_PUBKEY_PATH).join(config::RSA_PUBKEY_NAME);
    if let Ok(keys) = std::fs::read_to_string(&file_name) {
        let mut key_vec = vec![];
        let mut tmp_vec = vec![];

        for line in keys.split('\n') {
            if line.contains("BEGIN PUBLIC KEY") {
                tmp_vec.clear();
            }
            tmp_vec.push(line);
            if line.contains("END PUBLIC KEY") {
                key_vec.push(tmp_vec.join("\n"));
            }
        }

        hdc::debug!("read {} known hosts from file", key_vec.len());
        key_vec
    } else {
        hdc::info!("pubkey file {:#?} not exists", file_name);
        vec![]
    }
}

fn write_known_hosts_pubkey(pubkey: &String) -> io::Result<()> {
    let file_name = Path::new(config::RSA_PUBKEY_PATH).join(config::RSA_PUBKEY_NAME);
    if !file_name.exists() {
        hdc::info!("create pubkeys file at {:#?}", file_name);
        let _ = std::fs::create_dir_all(config::RSA_PUBKEY_PATH);
        let _ = std::fs::File::create(&file_name).unwrap();
    }

    let _ = match std::fs::File::options().append(true).open(file_name) {
        Ok(mut f) => write!(&mut f, "{}", pubkey),
        Err(e) => {
            hdc::error!("write pubkey err: {e}");
            return Err(e);
        }
    };
    Ok(())
}

fn require_user_permittion(_pubkey: &str) -> bool {
    // TODO: get user permittion from `settings`
    true
}

async fn handshake_fail(session_id: u32, channel_id: u32, msg: String) {
    AuthStatusMap::put(session_id, AuthStatus::Fail).await;
    let send = native_struct::SessionHandShake {
        banner: HANDSHAKE_MESSAGE.to_string(),
        session_id,
        auth_type: AuthType::Fail as u8,
        buf: msg,
        ..Default::default()
    };
    transfer::put(
        session_id,
        TaskMessage {
            channel_id,
            command: config::HdcCommand::KernelHandshake,
            payload: send.serialize(),
        },
    )
    .await;
}
