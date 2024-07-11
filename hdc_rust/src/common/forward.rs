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
//! forward
#![allow(missing_docs)]
#[cfg(feature = "host")]
extern crate ylong_runtime_static as ylong_runtime;

#[cfg(not(feature = "host"))]
use libc::SOCK_STREAM;
#[cfg(not(target_os = "windows"))]
use libc::{AF_LOCAL, AF_UNIX, FD_CLOEXEC, F_SETFD};
use std::collections::HashMap;
#[cfg(not(target_os = "windows"))]
use std::fs::{self, File, OpenOptions};
#[cfg(not(target_os = "windows"))]
use std::io::{self, Error, ErrorKind, Read, Write};
use ylong_runtime::sync::{Mutex, RwLock};

use crate::common::base::Base;
use crate::common::hdctransfer::transfer_task_finish;
use crate::common::hdctransfer::{self, HdcTransferBase};
#[cfg(not(feature = "host"))]
use crate::common::jdwp::Jdwp;
#[cfg(not(target_os = "windows"))]
use crate::common::uds::{UdsAddr, UdsClient, UdsServer};
use crate::config::HdcCommand;
use crate::config::MessageLevel;
use crate::config::TaskMessage;
use crate::transfer;
#[allow(unused)]
use crate::utils::hdc_log::*;
use crate::{config, utils};
use std::mem::MaybeUninit;
use std::sync::Arc;
use std::sync::Once;
#[cfg(not(feature = "host"))]
use std::time::Duration;
use ylong_runtime::io::AsyncReadExt;
use ylong_runtime::io::AsyncWriteExt;
use ylong_runtime::net::{SplitReadHalf, SplitWriteHalf, TcpListener, TcpStream};
use ylong_runtime::task::JoinHandle;

pub const ARG_COUNT2: u32 = 2;
pub const BUF_SIZE_SMALL: usize = 256;
pub const SOCKET_BUFFER_SIZE: usize = 65535;
pub const HARMONY_RESERVED_SOCKET_PREFIX: &str = "/dev/socket";
pub const FILE_SYSTEM_SOCKET_PREFIX: &str = "/tmp/";

#[cfg(feature = "host")]
#[derive(Clone, Debug)]
pub struct HdcForwardInfo {
    pub session_id: u32,
    pub channel_id: u32,
    pub forward_direction: bool,
    pub task_string: String,
    pub connect_key: String,
}

#[cfg(feature = "host")]
impl HdcForwardInfo {
    fn new(
        session_id: u32,
        channel_id: u32,
        forward_direction: bool,
        task_string: String,
        connect_key: String,
    ) -> Self {
        Self {
            session_id,
            channel_id,
            forward_direction,
            task_string,
            connect_key,
        }
    }
}

#[derive(Default, Eq, PartialEq, Clone, Debug)]
enum ForwardType {
    #[default]
    Tcp = 0,
    Device,
    Abstract,
    FileSystem,
    Jdwp,
    Ark,
    Reserved,
}

#[derive(Debug, Default, PartialEq, Eq, Clone)]
pub struct ContextForward {
    session_id: u32,
    channel_id: u32,
    check_order: bool,
    is_master: bool,
    id: u32,
    fd: i32,
    target_fd: i32,
    forward_type: ForwardType,
    local_args: Vec<String>,
    remote_args: Vec<String>,
    task_command: String,
    last_error: String,
    remote_parameters: String,
    dev_path: String,
}

#[cfg(feature = "host")]
type HdcForwardInfo_ = Arc<Mutex<HdcForwardInfo>>;
#[cfg(feature = "host")]
type HdcForwardInfoMap_ = Arc<Mutex<HashMap<String, HdcForwardInfo_>>>;
#[cfg(feature = "host")]
pub struct HdcForwardInfoMap {}
#[cfg(feature = "host")]
impl HdcForwardInfoMap {
    fn get_instance() -> HdcForwardInfoMap_ {
        static mut MAP: Option<HdcForwardInfoMap_> = None;
        unsafe {
            MAP.get_or_insert_with(|| Arc::new(Mutex::new(HashMap::new())))
                .clone()
        }
    }

    async fn put(forward_info: HdcForwardInfo) {
        let instance = Self::get_instance();
        let mut map = instance.lock().await;
        map.insert(
            forward_info.task_string.clone(),
            Arc::new(Mutex::new(forward_info)),
        );
    }

    pub async fn get_all_forward_infos() -> Vec<HdcForwardInfo> {
        let instance = Self::get_instance();
        let map = instance.lock().await;
        let mut result = Vec::new();
        for (_key, value) in map.iter() {
            let info = value.lock().await;
            result.push((*info).clone());
        }
        result
    }

    pub async fn remove_forward(task_string: String, forward_direction: bool) -> bool {
        crate::info!(
            "remove_forward task_string:{}, direction:{}",
            task_string,
            forward_direction
        );
        let instance = Self::get_instance();
        let map = instance.lock().await;
        let mut remove_key = String::new();
        let prefix = if forward_direction {
            "1|".to_string()
        } else {
            "0|".to_string()
        };
        let mut task_string1 = prefix;
        task_string1.push_str(task_string.as_str());
        for (key, value) in map.iter() {
            let info = value.lock().await;
            if info.task_string.contains(&task_string1)
                && info.forward_direction == forward_direction
            {
                remove_key = (*key.clone()).to_string();
                break;
            }
        }
        drop(map);
        if remove_key.is_empty() {
            return false;
        }

        let mut map = instance.lock().await;
        let result = map.remove(&remove_key);
        result.is_some()
    }
}

type TcpWriter = Arc<Mutex<SplitWriteHalf>>;
type TcpWriterMap_ = Arc<RwLock<HashMap<u32, TcpWriter>>>;
pub struct TcpWriteStreamMap {}
impl TcpWriteStreamMap {
    fn get_instance() -> TcpWriterMap_ {
        static mut TCP_MAP: Option<TcpWriterMap_> = None;
        unsafe {
            TCP_MAP
                .get_or_insert_with(|| Arc::new(RwLock::new(HashMap::new())))
                .clone()
        }
    }
    #[allow(unused)]
    async fn put(id: u32, wr: SplitWriteHalf) {
        let instance = Self::get_instance();
        let mut map = instance.write().await;
        let arc_wr = Arc::new(Mutex::new(wr));
        map.insert(id, arc_wr);
    }
    #[allow(unused)]
    async fn write(id: u32, data: Vec<u8>) -> bool {
        let arc_map = Self::get_instance();
        let map = arc_map.write().await;
        let Some(arc_wr) = map.get(&id) else {
            crate::error!("TcpWriteStreamMap failed to get id {:#?}", id);
            return false;
        };
        let mut wr = arc_wr.lock().await;
        let write_result = wr.write_all(data.as_slice()).await;
        if write_result.is_err() {
            crate::error!("TcpWriteStreamMap write_all error. id = {:#?}", id);
        }
        true
    }

    pub async fn end(id: u32) {
        let instance = Self::get_instance();
        let mut map = instance.write().await;
        if let Some(arc_wr) = map.remove(&id) {
            let mut wr = arc_wr.lock().await;
            let _ = wr.shutdown().await;
        }
    }
}

type TcpListener_ = Arc<Mutex<JoinHandle<()>>>;
type TcpListenerMap_ = Arc<RwLock<HashMap<u32, TcpListener_>>>;
pub struct TcpListenerMap {}
impl TcpListenerMap {
    fn get_instance() -> TcpListenerMap_ {
        static mut TCP_MAP: Option<TcpListenerMap_> = None;
        unsafe {
            TCP_MAP
                .get_or_insert_with(|| Arc::new(RwLock::new(HashMap::new())))
                .clone()
        }
    }
    #[allow(unused)]
    async fn put(id: u32, listener: JoinHandle<()>) {
        let instance = Self::get_instance();
        let mut map = instance.write().await;
        let arc_listener = Arc::new(Mutex::new(listener));
        map.insert(id, arc_listener);
        crate::info!("forward tcp put listener id = {id}");
    }

    pub async fn end(id: u32) {
        let instance = Self::get_instance();
        let mut map = instance.write().await;
        if let Some(arc_listener) = map.remove(&id) {
            let join_handle = arc_listener.lock().await;
            join_handle.cancel();
        }
    }
}

type MapContextForward_ = Arc<Mutex<HashMap<u32, ContextForward>>>;
pub struct ForwardContextMap {}
impl ForwardContextMap {
    fn get_instance() -> &'static MapContextForward_ {
        static mut CONTEXT_MAP: MaybeUninit<MapContextForward_> = MaybeUninit::uninit();
        static ONCE: Once = Once::new();
        unsafe {
            ONCE.call_once(|| {
                    CONTEXT_MAP = MaybeUninit::new(Arc::new(Mutex::new(HashMap::new())));
                }
            );
            &*CONTEXT_MAP.as_ptr()
        }
    }

    pub async fn update(cid: u32, value: ContextForward) {
        let map = Self::get_instance();
        let mut map = map.lock().await;
        map.insert(cid, value.clone());
    }

    pub async fn remove(cid: u32) {
        crate::info!("ContextForward remove, cid:{}", cid);
        let map = Self::get_instance();
        let mut map = map.lock().await;
        let _ = map.remove(&cid);
    }

    pub async fn get(cid: u32) -> Option<ContextForward> {
        let arc = Self::get_instance();
        let map = arc.lock().await;
        let task = map.get(&cid);
        match task {
            Some(task) => Some(task.clone()),
            None => {
                crate::debug!("ContextForward result:is none,cid={:#?}", cid,);
                Option::None
            }
        }
    }
}

type MapForward_ = Arc<Mutex<HashMap<(u32, u32), HdcForward>>>;
pub struct ForwardTaskMap {}
impl ForwardTaskMap {
    fn get_instance() -> MapForward_ {
        static mut FORWARD_MAP: Option<MapForward_> = None;
        unsafe {
            FORWARD_MAP
                .get_or_insert_with(|| Arc::new(Mutex::new(HashMap::new())))
                .clone()
        }
    }

    pub async fn update(session_id: u32, channel_id: u32, value: HdcForward) {
        let map = Self::get_instance();
        let mut map = map.lock().await;
        map.insert((session_id, channel_id), value.clone());
    }

    pub async fn remove(session_id: u32, channel_id: u32) {
        crate::info!("remove, session:{}, channel:{}", session_id, channel_id);
        let map = Self::get_instance();
        let mut map = map.lock().await;
        let _ = map.remove(&(session_id, channel_id));
    }

    pub async fn get(session_id: u32, channel_id: u32) -> Option<HdcForward> {
        let arc = Self::get_instance();
        let map = arc.lock().await;
        let task = map.get(&(session_id, channel_id));
        match task {
            Some(task) => Some(task.clone()),
            None => {
                crate::debug!(
                    "Forward TaskMap result:is none,session_id={:#?}, channel_id={:#?}",
                    session_id,
                    channel_id
                );
                Option::None
            }
        }
    }

    pub async fn get_channel_id(session_id: u32, task_string: String) -> Option<u32> {
        let arc = Self::get_instance();
        let map = arc.lock().await;
        for ((_session_id, _channel_id), value) in map.iter() {
            if *_session_id == session_id && task_string.contains(&value.task_command) {
                return Some(*_channel_id);
            }
        }
        None
    }

    pub async fn clear(session_id: u32) {
        let arc = Self::get_instance();
        let mut channel_list = Vec::new();
        {
            let map = arc.lock().await;
            if map.is_empty() {
                return;
            }
            for (&key, _) in map.iter() {
                if key.0 == session_id {
                    let id = key;
                    channel_list.push(id);
                }
            }
        }
        for id in channel_list {
            free_channel_task(id.0, id.1).await;
        }
    }

    pub async fn dump_task() -> String {
        let arc = Self::get_instance();
        let map = arc.lock().await;
        let mut result = String::new();
        for (_id, forward_task) in map.iter() {
            let forward_type = match forward_task.remote_args.len() {
                0 => "fport".to_string(),
                2 => "rport".to_string(),
                _ => "unknown".to_string(),
            };
            let first_args = match forward_task.remote_args.len() {
                0 => "unknown".to_string(),
                2 => format!(
                    "{}:{}",
                    forward_task.local_args[0], forward_task.local_args[1]
                ),
                _ => "unknown".to_string(),
            };
            let second_args = match forward_task.remote_args.len() {
                0 => format!(
                    "{}:{}",
                    forward_task.local_args[0], forward_task.local_args[1]
                ),
                2 => format!(
                    "{}:{}",
                    forward_task.remote_args[0], forward_task.remote_args[1]
                ),
                _ => "unknown".to_string(),
            };
            result.push_str(&format!(
                "session_id:{},\tchannel_id:{},\tcommand:{:#} {:#} {:#}\n",
                forward_task.session_id,
                forward_task.channel_id,
                forward_type,
                first_args,
                second_args
            ));
        }
        result
    }
}

pub async fn free_channel_task(session_id: u32, channel_id: u32) {
    let Some(task) = ForwardTaskMap::get(session_id, channel_id).await else {
        return;
    };
    let task = &mut task.clone();
    let cid = task.context_forward.id;
    crate::info!("free channel context session_id:{session_id}, channel_id:{channel_id}, cid:{cid}");

    match task.forward_type {
        ForwardType::Tcp => {
            TcpWriteStreamMap::end(cid).await;
            TcpListenerMap::end(channel_id).await;
        }
        ForwardType::Jdwp | ForwardType::Ark => {
            TcpWriteStreamMap::end(cid).await;
            let ret = unsafe { libc::close(task.context_forward.fd) };
            crate::debug!(
                "close context_forward fd, ret={}, session_id={}, channel_id={}",
                ret,
                session_id,
                channel_id,
            );
            let target_fd_ret = unsafe { libc::close(task.context_forward.target_fd) };
            crate::debug!(
                "close context_forward target fd, ret={}, session_id={}, channel_id={}",
                target_fd_ret,
                session_id,
                channel_id,
            );
            TcpListenerMap::end(channel_id).await;
        }
        ForwardType::Abstract | ForwardType::FileSystem | ForwardType::Reserved => {
            #[cfg(not(target_os = "windows"))]
            UdsServer::wrap_close(task.context_forward.fd);
        }
        ForwardType::Device => {
            return;
        }
    }
    ForwardTaskMap::remove(session_id, channel_id).await;
}

pub async fn stop_task(session_id: u32) {
    ForwardTaskMap::clear(session_id).await;
}

pub async fn dump_task() -> String {
    ForwardTaskMap::dump_task().await
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct HdcForward {
    session_id: u32,
    channel_id: u32,
    server_or_daemon: bool,
    local_args: Vec<String>,
    remote_args: Vec<String>,
    task_command: String,
    forward_type: ForwardType,
    context_forward: ContextForward,
    pub transfer: HdcTransferBase,
}

impl HdcForward {
    pub fn new(session_id: u32, channel_id: u32, server_or_daemon: bool) -> Self {
        Self {
            session_id,
            channel_id,
            server_or_daemon,
            local_args: Default::default(),
            remote_args: Default::default(),
            task_command: Default::default(),
            forward_type: Default::default(),
            context_forward: Default::default(),
            transfer: HdcTransferBase::new(session_id, channel_id),
        }
    }
}

pub fn check_node_info(value: &String, arg: &mut Vec<String>) -> bool {
    crate::info!("check cmd args is : {:#?}", value);
    if !value.contains(':') {
        return false;
    }
    let array = value.split(':').collect::<Vec<&str>>();
    if array[0] == "tcp" {
        if array[1].len() > config::MAX_PORT_LEN {
            crate::error!(
                "forward port = {:#?} it'slength is wrong, can not more than five",
                array[1]
            );
            return false;
        }

        match array[1].parse::<u32>() {
            Ok(port) => {
                if port == 0 || port > config::MAX_PORT_NUM {
                    crate::error!("port can not greater than: 65535");
                    return false;
                }
            }
            Err(_) => {
                crate::error!("port must is int type, port is: {:#?}", array[1]);
                return false;
            }
        }
    }
    for item in array.iter() {
        arg.push(String::from(item.to_owned()));
    }
    true
}

#[cfg(feature = "host")]
pub async fn on_forward_success(task_message: TaskMessage, session_id: u32) -> io::Result<()> {
    crate::info!("on_forward_success");
    let channel_id = task_message.channel_id;
    let payload = task_message.payload;
    let forward_direction = payload[0] == b'1';
    let connect_key = "unknow key".to_string();
    let task_string = String::from_utf8(payload);
    match task_string {
        Ok(task_string) => {
            let info = HdcForwardInfo::new(
                session_id,
                channel_id,
                forward_direction,
                task_string,
                connect_key,
            );
            HdcForwardInfoMap::put(info).await;
        }
        Err(err) => {
            crate::error!("payload to String failed. {err}");
        }
    }
    transfer::TcpMap::end(task_message.channel_id).await;
    Ok(())
}

pub async fn check_command(
    ctx: &mut ContextForward,
    _payload: &[u8],
    server_or_daemon: bool,
) -> bool {
    let channel_id = ctx.channel_id;
    if !_payload.is_empty() {
        hdctransfer::echo_client(
            ctx.session_id,
            channel_id,
            "Forwardport result:OK",
            MessageLevel::Ok,
        )
        .await;
        let map_info = String::from(if server_or_daemon { "1|" } else { "0|" }) + &ctx.task_command;

        let mut command_string = vec![0_u8; map_info.len() + 1];
        map_info
            .as_bytes()
            .to_vec()
            .iter()
            .enumerate()
            .for_each(|(i, e)| {
                command_string[i] = *e;
            });
        let forward_success_message = TaskMessage {
            channel_id,
            command: HdcCommand::ForwardSuccess,
            payload: command_string,
        };
        #[cfg(feature = "host")]
        {
            let _ = on_forward_success(forward_success_message, ctx.session_id).await;
        }
        #[cfg(not(feature = "host"))]
        {
            transfer::put(ctx.session_id, forward_success_message).await;
        }
    } else {
        hdctransfer::echo_client(
            ctx.session_id,
            channel_id,
            "Forwardport result: Failed",
            MessageLevel::Fail,
        )
        .await;
        free_context(ctx.id, false).await;
        return false;
    }
    true
}

pub async fn detech_forward_type(ctx_point: &mut ContextForward) -> bool {
    let type_str = &ctx_point.local_args[0];
    match type_str.as_str() {
        "tcp" => {
            ctx_point.forward_type = ForwardType::Tcp;
        }
        "dev" => {
            ctx_point.forward_type = ForwardType::Device;
        }
        "localabstract" => {
            ctx_point.forward_type = ForwardType::Abstract;
        }
        "localfilesystem" => {
            ctx_point.local_args[1] =
                HARMONY_RESERVED_SOCKET_PREFIX.to_owned() + &ctx_point.local_args[1];
            ctx_point.forward_type = ForwardType::FileSystem;
        }
        "jdwp" => {
            ctx_point.forward_type = ForwardType::Jdwp;
        }
        "ark" => {
            ctx_point.forward_type = ForwardType::Ark;
        }
        "localreserved" => {
            ctx_point.local_args[1] =
                FILE_SYSTEM_SOCKET_PREFIX.to_owned() + &ctx_point.local_args[1];
            ctx_point.forward_type = ForwardType::Reserved;
        }
        _ => {
            crate::error!("this forward type may is not expected");
            return false;
        }
    }
    true
}

pub async fn forward_tcp_accept(ctx: &mut ContextForward, port: u32) -> io::Result<()> {
    let saddr = format!("127.0.0.1:{}", port);
    let session_tmp = ctx.session_id;
    let channel_tmp = ctx.channel_id;
    let remote_parameters = ctx.remote_parameters.clone();
    let ctx_type = ctx.forward_type.clone();

    let result = TcpListener::bind(saddr.clone()).await;
    match result {
        Ok(listener) => {
            crate::info!("forward tcp accept bind ok, addr:{:#?}", saddr);
            let join_handle = utils::spawn(async move {
                loop {
                    let (stream, _addr) = match listener.accept().await {
                        Ok((stream, _addr)) => (stream, _addr),
                        Err(err) => {
                            crate::error!("listener accept failed, {err}");
                            continue;
                        }
                    };
                    let (rd, wr) = stream.into_split();
                    let mut client_context = malloc_context(session_tmp, channel_tmp, true).await;
                    client_context.remote_parameters = remote_parameters.clone();
                    client_context.forward_type = ctx_type.clone();
                    ForwardContextMap::update(client_context.id, client_context.clone()).await;
                    crate::info!("malloc client_context id = {:#?}", client_context.id);
                    TcpWriteStreamMap::put(client_context.id, wr).await;
                    on_accept(client_context.id).await;
                    utils::spawn(async move {
                        recv_tcp_msg(session_tmp, channel_tmp, rd, client_context.id).await;
                    });
                }
            });
            TcpListenerMap::put(channel_tmp, join_handle).await;
            Ok(())
        }
        Err(e) => {
            crate::error!("forward_ tcp_accept fail:{:#?}", e);
            ctx.last_error = format!("TCP Port listen failed at {}", port);
            Err(e)
        }
    }
}

pub async fn on_accept(cid: u32) {
    let Some(context_forward) = ForwardContextMap::get(cid).await else {
        crate::error!("daemon_connect _tcp2 get context is none cid={cid}");
        return;
    };
    let ctx = &mut context_forward.clone();

    let buf_string: Vec<u8> = ctx.remote_parameters.clone().as_bytes().to_vec();
    let mut new_buf = vec![0_u8; buf_string.len() + 9];

    buf_string.iter().enumerate().for_each(|(i, e)| {
        new_buf[i + 8] = *e;
    });
    send_to_task(
        ctx.session_id,
        ctx.channel_id,
        HdcCommand::ForwardActiveSlave,
        &new_buf,
        buf_string.len() + 9,
        ctx.id,
    )
    .await;
}

pub async fn daemon_connect_tcp(cid: u32, port: u32) {
    let Some(context_forward) = ForwardContextMap::get(cid).await else {
        crate::error!("daemon connect tcp get context is none cid={cid}");
        return;
    };
    let ctx = &mut context_forward.clone();

    let saddr = format!("127.0.0.1:{}", port);
    let stream = match TcpStream::connect(saddr).await {
        Err(err) => {
            crate::error!("TcpStream::stream failed {:?}", err);
            free_context(cid, true).await;
            return;
        }
        Ok(addr) => addr,
    };
    send_active_master(ctx).await;
    let (rd, wr) = stream.into_split();
    TcpWriteStreamMap::put(ctx.id, wr).await;
    ForwardContextMap::update(ctx.id, ctx.clone()).await;

    let session_tmp = ctx.session_id;
    let channel_tmp = ctx.channel_id;
    update_context_to_task(session_tmp, channel_tmp, ctx).await;
    utils::spawn(async move {
        recv_tcp_msg(session_tmp, channel_tmp, rd, cid).await;
    });
}

pub async fn update_context_to_task(session_id: u32, channel_id:u32, ctx: &mut ContextForward) {
    let Some(task) = ForwardTaskMap::get(ctx.session_id, ctx.channel_id).await else {
        crate::error!(
            "update context to task is none session_id={:#?},channel_id={:#?}",
            session_id,
            channel_id
        );
        return;
    };
    let task = &mut task.clone(); 
    task.context_forward = ctx.clone();
    ForwardTaskMap::update(session_id, channel_id, task.clone()).await;
}

pub async fn recv_tcp_msg(session_id: u32, channel_id: u32, mut rd: SplitReadHalf, cid: u32) {
    let mut data = vec![0_u8; SOCKET_BUFFER_SIZE];
    loop {
        match rd.read(&mut data).await {
            Ok(recv_size) => {
                if recv_size == 0 {
                    crate::info!("recv_size is 0, tcp temporarily closed");
                    free_context(cid, true).await;
                    return;
                }
                send_to_task(
                    session_id,
                    channel_id,
                    HdcCommand::ForwardData,
                    &data[0..recv_size],
                    recv_size,
                    cid,
                )
                .await;
            }
            Err(_e) => {
                free_context(cid, true).await;
                crate::error!(
                    "recv tcp msg read failed session_id={session_id},channel_id={channel_id}"
                );
            }
        }
    }
}

#[cfg(not(target_os = "windows"))]
pub async fn deamon_read_socket_msg(session_id: u32, channel_id: u32, fd: i32, cid: u32) {
    loop {
        let result = ylong_runtime::spawn_blocking(move || {
            let mut buffer: [u8; SOCKET_BUFFER_SIZE] = [0; SOCKET_BUFFER_SIZE];
            let recv_size = UdsClient::wrap_recv(fd, &mut buffer);
            (recv_size, buffer)
        })
        .await;
        let (recv_size, buffer) = match result {
            Ok((recv_size, _)) if recv_size < 0 => {
                crate::error!("local abstract close shutdown fd = {fd}");
                free_context(cid, true).await;
                return;
            }
            Ok((recv_size, buffer)) => (recv_size, buffer),
            Err(err) => {
                crate::error!("read socket msg failed. {err}");
                free_context(cid, true).await;
                return;
            }
        };
        send_to_task(
            session_id,
            channel_id,
            HdcCommand::ForwardData,
            &buffer[0..recv_size as usize],
            recv_size as usize,
            cid,
        )
        .await;
    }
}

pub async fn free_context(cid: u32, notify_remote: bool) {
    let Some(context_forward) = ForwardContextMap::get(cid).await else {
        crate::error!("free forward context get cid  is none. cid = {cid}");
        return;
    };
    let ctx = &mut context_forward.clone();

    if notify_remote {
        let vec_none = Vec::<u8>::new();
        send_to_task(
            ctx.session_id,
            ctx.channel_id,
            HdcCommand::ForwardFreeContext,
            &vec_none,
            0,
            ctx.id,
        )
        .await;
    }
    crate::error!("begin to free forward context cid. cid = {cid}");
    match ctx.forward_type {
        ForwardType::Tcp => {
            TcpWriteStreamMap::end(ctx.id).await;
        }
        ForwardType::Jdwp | ForwardType::Ark => {
            TcpWriteStreamMap::end(ctx.id).await;
            let ret = unsafe { libc::close(ctx.fd) };
            crate::debug!("close context_forward fd, ret={}, id={}", ret, ctx.id,);
            let target_fd_ret = unsafe { libc::close(ctx.target_fd) };
            crate::debug!(
                "close context_forward target fd, ret={}, id={}",
                target_fd_ret,
                ctx.id,
            );
        }
        ForwardType::Abstract | ForwardType::FileSystem | ForwardType::Reserved => {
            crate::error!("Abstract begin to free forward close fd = {:#?}", ctx.fd);
            #[cfg(not(target_os = "windows"))]
            UdsServer::wrap_close(ctx.fd);
            ctx.fd = -1;
        }
        ForwardType::Device => {}
    }
    ForwardContextMap::remove(cid).await;
}

pub async fn setup_tcp_point(ctx: &mut ContextForward) -> bool {
    let Ok(port) = ctx.local_args[1].parse::<u32>() else {
        crate::error!("setup tcp point parse error");
        return false;
    };
    let cid = ctx.id;
    if ctx.is_master {
        let result = forward_tcp_accept(ctx, port).await;
        return result.is_ok();
    } else {
        ForwardContextMap::update(ctx.id, ctx.clone()).await;
        utils::spawn(async move { daemon_connect_tcp(cid, port).await });
    }
    true
}

#[cfg(not(target_os = "windows"))]
async fn server_socket_bind_listen(ctx: &mut ContextForward, path: String) -> bool {
    let fd: i32 = UdsClient::wrap_socket(AF_UNIX);
    ctx.fd = fd;
    let name: Vec<u8> = path.as_bytes().to_vec();
    let mut socket_name = vec![0_u8; name.len() + 1];
    socket_name[0] = b'\0';
    name.iter().enumerate().for_each(|(i, e)| {
        socket_name[i + 1] = *e;
    });
    let addr = UdsAddr::parse_abstract(&socket_name[1..]);
    ForwardContextMap::update(ctx.id, ctx.clone()).await;
    let cid = ctx.id;
    if let Ok(addr_obj) = &addr {
        let ret = UdsServer::wrap_bind(fd, addr_obj);
        if ret.is_err() {
            hdctransfer::echo_client(
                ctx.session_id,
                ctx.channel_id,
                "Unix pipe bind failed",
                MessageLevel::Fail,
            )
            .await;
            crate::error!("bind fail");
            return false;
        }
        let ret = UdsServer::wrap_listen(fd);
        if ret < 0 {
            hdctransfer::echo_client(
                ctx.session_id,
                ctx.channel_id,
                "Unix pipe listen failed",
                MessageLevel::Fail,
            )
            .await;
            crate::error!("listen fail");
            return false;
        }
        utils::spawn(async move {
            loop {
                let client_fd = UdsServer::wrap_accept(fd);
                if client_fd == -1 {
                    break;
                }
                on_accept(cid).await;
            }
        });
    }
    true
}

pub async fn canonicalize(path: String) -> Result<String, Error> {
    match fs::canonicalize(path) {
        Ok(abs_path) => match abs_path.to_str() {
            Some(path) => Ok(path.to_string()),
            None => Err(Error::new(ErrorKind::Other, "forward canonicalize failed")),
        },
        Err(_) => Err(Error::new(ErrorKind::Other, "forward canonicalize failed")),
    }
}

#[cfg(target_os = "windows")]
pub async fn setup_device_point(_ctx: &mut ContextForward) -> bool {
    false
}

#[cfg(not(target_os = "windows"))]
pub async fn setup_device_point(ctx: &mut ContextForward) -> bool {
    let s_node_cfg = ctx.local_args[1].clone();
    let Ok(resolv_path) = canonicalize(s_node_cfg).await else {
        crate::error!("Open unix-dev failed");
        return false;
    };
    ctx.dev_path = resolv_path.clone();
    crate::info!("setup_ device_point resolv_path={:?}", resolv_path);
    let thread_path_ref = Arc::new(Mutex::new(resolv_path));
    if !send_active_master(ctx).await {
        crate::error!("send active_master return failed ctx={:?}", ctx);
        return false;
    }
    let session = ctx.session_id;
    let channel = ctx.channel_id;
    let cid = ctx.id;

    ForwardContextMap::update(ctx.id, ctx.clone()).await;
    utils::spawn(async move {
        loop {
            let path = thread_path_ref.lock().await;
            let Ok(mut file) = File::open(&*path) else {
                crate::error!("open {} failed.", *path);
                break;
            };
            let mut total = Vec::new();
            let mut buf: [u8; config::FILE_PACKAGE_PAYLOAD_SIZE] =
                [0; config::FILE_PACKAGE_PAYLOAD_SIZE];
            let Ok(read_len) = file.read(&mut buf[4..]) else {
                crate::error!("read {} failed.", *path);
                break;
            };
            if read_len == 0 {
                free_context(cid, true).await;
                break;
            }
            total.append(&mut buf[0..read_len].to_vec());
            send_to_task(
                session,
                channel,
                HdcCommand::ForwardData,
                &total,
                read_len,
                cid,
            )
            .await;
        }
    });
    true
}

#[cfg(not(feature = "host"))]
fn get_pid(parameter: &str, forward_type: ForwardType) -> u32 {
    match forward_type == ForwardType::Jdwp {
        true => parameter.parse::<u32>().unwrap_or_else(|e| {
            crate::error!("Jdwp get pid err :{:?}", e);
            0_u32
        }),
        false => {
            let params: Vec<&str> = parameter.split('@').collect();
            params[0].parse::<u32>().unwrap_or_else(|e| {
                crate::error!("get pid err :{:?}", e);
                0_u32
            })
        }
    }
}

#[cfg(feature = "host")]
pub async fn setup_jdwp_point(_ctx: &mut ContextForward) -> bool {
    crate::info!("host not setup_jdwp _point");
    false
}

#[cfg(not(feature = "host"))]
pub async fn setup_jdwp_point(ctx: &mut ContextForward) -> bool {
    let local_args = ctx.local_args[1].clone();
    let parameter = local_args.as_str();
    let style = &ctx.forward_type;
    let pid = get_pid(parameter, style.clone());
    let cid = ctx.id;
    let session = ctx.session_id;
    let channel = ctx.channel_id;
    if pid == 0 {
        crate::error!("setup jdwp point get pid is 0");
        return false;
    }

    let result = UdsServer::wrap_socketpair(SOCK_STREAM);
    if result.is_err() {
        crate::error!("wrap socketpair failed");
        return false;
    }
    let mut target_fd = 0;
    let mut local_fd = 0;
    if let Ok((fd0, fd1)) = result {
        crate::info!("pipe, fd0:{}, fd1:{}", fd0, fd1);
        local_fd = fd0;
        target_fd = fd1;
        ctx.fd = local_fd;
        ctx.target_fd = target_fd;
        target_fd = fd1;
    }

    utils::spawn(async move {
        loop {
            let result = ylong_runtime::spawn_blocking(move || {
                let mut buffer = [0u8; SOCKET_BUFFER_SIZE];
                let size = UdsServer::wrap_read(local_fd, &mut buffer);
                (size, buffer)
            })
            .await;
            let (size, buffer) = match result {
                Ok((size, _)) if size < 0 => {
                    crate::error!("disconnect fd:({local_fd}, {target_fd}), error:{:?}", size);
                    free_context(cid, true).await;
                    break;
                }
                Ok((0, _)) => {
                    ylong_runtime::time::sleep(Duration::from_millis(200)).await;
                    continue;
                }
                Ok((size, buffer)) => (size, buffer),
                Err(err) => {
                    crate::error!("spawn_blocking failed. disconnect fd:({local_fd}, {target_fd}), error:{err}");
                    free_context(cid, true).await;
                    break;
                }
            };
            send_to_task(
                session,
                channel,
                HdcCommand::ForwardData,
                &buffer[0..size as usize],
                size as usize,
                cid,
            )
            .await;
        }
    });

    let jdwp = Jdwp::get_instance();
    let mut param = ctx.local_args[0].clone();
    param.push(':');
    param.push_str(parameter);

    let ret = jdwp
        .send_fd_to_target(pid, target_fd, local_fd, param.as_str())
        .await;
    if !ret {
        crate::error!("not found pid:{:?}", pid);
        hdctransfer::echo_client(
            session,
            channel,
            format!("fport fail:pid not found:{}", pid).as_str(),
            MessageLevel::Fail,
        )
        .await;
        task_finish(session, channel).await;
        return false;
    }

    let vec_none = Vec::<u8>::new();
    send_to_task(
        session,
        channel,
        HdcCommand::ForwardActiveMaster, // 04
        &vec_none,
        0,
        cid,
    )
    .await;
    crate::info!("setup_jdwp_ point return true");
    true
}

async fn task_finish(session_id: u32, channel_id: u32) {
    transfer_task_finish(channel_id, session_id).await;
}

#[cfg(not(target_os = "windows"))]
pub async fn daemon_connect_pipe(ctx: &mut ContextForward) {
    let name: Vec<u8> = ctx.local_args[1].clone().as_bytes().to_vec();
    let mut socket_name = vec![0_u8; name.len() + 1];
    socket_name[0] = b'\0';
    name.iter().enumerate().for_each(|(i, e)| {
        socket_name[i + 1] = *e;
    });
    let addr = UdsAddr::parse_abstract(&socket_name[1..]);
    if let Ok(addr_obj) = &addr {
        let ret: Result<(), Error> = UdsClient::wrap_connect(ctx.fd, addr_obj);
        if ret.is_err() {
            hdctransfer::echo_client(
                ctx.session_id,
                ctx.channel_id,
                "localabstract connect fail",
                MessageLevel::Fail,
            )
            .await;
            free_context(ctx.id, true).await;
            return;
        }
        send_active_master(ctx).await;
        read_data_to_forward(ctx).await;
    }
}

#[cfg(target_os = "windows")]
pub async fn setup_file_point(_ctx: &mut ContextForward) -> bool {
    false
}

#[cfg(not(target_os = "windows"))]
pub async fn setup_file_point(ctx: &mut ContextForward) -> bool {
    let s_node_cfg = ctx.local_args[1].clone();
    if ctx.is_master {
        if ctx.forward_type == ForwardType::Reserved || ctx.forward_type == ForwardType::FileSystem
        {
            let _ = fs::remove_file(s_node_cfg.clone());
        }
        if !server_socket_bind_listen(ctx, s_node_cfg).await {
            crate::error!("server socket bind listen failed id={:?}", ctx.id);
            task_finish(ctx.session_id, ctx.channel_id).await;
            return false;
        }
    } else {
        if ctx.fd <= 0 {
            crate::info!("setup_file _point fd: {:?}", ctx.fd);
            if ctx.forward_type == ForwardType::Abstract {
                ctx.fd = UdsClient::wrap_socket(AF_LOCAL);
                unsafe {
                    libc::fcntl(ctx.fd, F_SETFD, FD_CLOEXEC);
                }
            } else {
                ctx.fd = UdsClient::wrap_socket(AF_UNIX);
            }
        }
        ForwardContextMap::update(ctx.id, ctx.clone()).await;
        daemon_connect_pipe(ctx).await;
    }
    true
}

pub async fn setup_point(ctx: &mut ContextForward) -> bool {
    if !detech_forward_type(ctx).await {
        crate::error!("forward type is not true");
        return false;
    }

    if cfg!(target_os = "windows") && ctx.forward_type != ForwardType::Tcp {
        ctx.last_error = String::from("Not support forward-type");
        return false;
    }

    let mut ret = false;
    match ctx.forward_type {
        ForwardType::Tcp => {
            ret = setup_tcp_point(ctx).await;
        }
        ForwardType::Device => {
            if !cfg!(target_os = "windows") {
                ret = setup_device_point(ctx).await;
            }
        }
        ForwardType::Jdwp | ForwardType::Ark => {
            crate::info!("setup point ark case");
            if !cfg!(feature = "host") {
                ret = setup_jdwp_point(ctx).await;
            }
        }
        ForwardType::Abstract | ForwardType::FileSystem | ForwardType::Reserved => {
            if !cfg!(target_os = "windows") {
                ret = setup_file_point(ctx).await;
            }
        }
    };
    ForwardContextMap::update(ctx.id, ctx.clone()).await;
    update_context_to_task(ctx.session_id, ctx.channel_id, ctx).await;
    ret
}

pub async fn send_to_task(
    session_id: u32,
    channel_id: u32,
    command: HdcCommand,
    buf_ptr: &[u8],
    buf_size: usize,
    cid: u32,
) -> bool {
    if buf_size > (config::MAX_SIZE_IOBUF * 2) {
        crate::error!("send task buf_size oversize");
        return false;
    }

    let mut new_buf = [u32::to_be_bytes(cid).as_slice(), buf_ptr].concat();
    new_buf[4..].copy_from_slice(&buf_ptr[0..buf_size]);
    let file_check_message = TaskMessage {
        channel_id,
        command,
        payload: new_buf,
    };
    transfer::put(session_id, file_check_message).await;
    true
}

pub fn get_cid(_payload: &[u8]) -> u32 {
    let mut id_bytes = [0u8; 4];
    id_bytes.copy_from_slice(&_payload[0..4]);
    let id: u32 = u32::from_be_bytes(id_bytes);
    id
}

pub async fn send_active_master(ctx: &mut ContextForward) -> bool {
    if ctx.check_order {
        let flag = [0u8; 1];
        send_to_task(
            ctx.session_id,
            ctx.channel_id,
            HdcCommand::ForwardCheckResult,
            &flag,
            1,
            ctx.id,
        )
        .await;
        free_context(ctx.id, false).await;
        return true;
    }
    if !send_to_task(
        ctx.session_id,
        ctx.channel_id,
        HdcCommand::ForwardActiveMaster,
        &Vec::<u8>::new(),
        0,
        ctx.id,
    )
    .await
    {
        free_context(ctx.id, true).await;
        return false;
    }
    true
}

pub fn forward_parse_cmd(context_forward: &mut ContextForward) -> bool {
    let command = context_forward.task_command.clone();
    let result = Base::split_command_to_args(&command);
    let argv = result.0;
    let argc = result.1;

    if argc < ARG_COUNT2 {
        crate::error!("argc < 2 parse is failed.");
        context_forward.last_error = "Too few arguments.".to_string();
        return false;
    }
    if argv[0].len() > BUF_SIZE_SMALL || argv[1].len() > BUF_SIZE_SMALL {
        crate::error!("parse's length is flase.");
        context_forward.last_error = "Some argument too long.".to_string();
        return false;
    }
    if !check_node_info(&argv[0], &mut context_forward.local_args) {
        crate::error!("check argv[0] node info is flase.");
        context_forward.last_error = "Arguments parsing failed.".to_string();
        return false;
    }
    if !check_node_info(&argv[1], &mut context_forward.remote_args) {
        crate::error!("check argv[1] node info is flase.");
        context_forward.last_error = "Arguments parsing failed.".to_string();
        return false;
    }
    context_forward.remote_parameters = argv[1].clone();
    true
}

pub async fn begin_forward(session_id: u32, channel_id: u32, _payload: &[u8]) -> bool {
    let Some(task) = ForwardTaskMap::get(session_id, channel_id).await else {
        crate::error!("begin forward get task is none session_id={session_id},channel_id={channel_id}"
        );
        return false;
    };
    let task: &mut HdcForward = &mut task.clone();
    let Ok(command) = String::from_utf8(_payload.to_vec()) else {
        crate::error!("cmd argv is not int utf8");
        return false;
    };
    let mut context_forward = malloc_context(session_id, channel_id, true).await;
    context_forward.task_command = command.clone();

    if !forward_parse_cmd(&mut context_forward) {
        task.context_forward = context_forward.clone();
        ForwardTaskMap::update(session_id, channel_id, task.clone()).await;
        return false;
    }
    if !setup_point(&mut context_forward).await {
        task.context_forward = context_forward.clone();
        ForwardTaskMap::update(session_id, channel_id, task.clone()).await;
        return false;
    }

    let wake_up_message = TaskMessage {
        channel_id,
        command: HdcCommand::KernelWakeupSlavetask,
        payload: Vec::<u8>::new(),
    };
    transfer::put(context_forward.session_id, wake_up_message).await;

    let buf_string: Vec<u8> = context_forward.remote_parameters.as_bytes().to_vec();
    let mut new_buf = vec![0_u8; buf_string.len() + 9];
    buf_string.iter().enumerate().for_each(|(i, e)| {
        new_buf[i + 8] = *e;
    });
    send_to_task(
        context_forward.session_id,
        context_forward.channel_id,
        HdcCommand::ForwardCheck,
        &new_buf,
        buf_string.len() + 9,
        context_forward.id,
    )
    .await;
    true
}

pub async fn slave_connect(
    session_id: u32,
    channel_id: u32,
    payload: &[u8],
    check_order: bool,
) -> bool {
    let mut context_forward = malloc_context(session_id, channel_id, false).await;
    context_forward.check_order = check_order;
    if let Ok((content, id)) = filter_command(payload) {
        let content = &content[8..].trim_end_matches('\0').to_string();
        context_forward.task_command = content.clone();
        if !check_node_info(content, &mut context_forward.local_args) {
            crate::error!("check local args is false");
            return false;
        }
        context_forward.id = id;
        ForwardContextMap::update(id, context_forward.clone()).await;
    }

    if !check_order {
        if !setup_point(&mut context_forward).await {
            crate::error!("setup point return false, free context");
            free_context(context_forward.id, true).await;
            ForwardContextMap::update(context_forward.id, context_forward.clone()).await;
            return false;
        }
    } else {
        send_active_master(&mut context_forward).await;
    }
    ForwardContextMap::update(context_forward.id, context_forward.clone()).await;
    true
}

pub async fn read_data_to_forward(ctx: &mut ContextForward) {
    let cid = ctx.id;
    let session = ctx.session_id;
    let channel = ctx.channel_id;
    match ctx.forward_type {
        ForwardType::Abstract | ForwardType::FileSystem | ForwardType::Reserved => {
            #[cfg(not(target_os = "windows"))]
            {
                let fd_temp = ctx.fd;
                utils::spawn(async move {
                    deamon_read_socket_msg(session, channel, fd_temp, cid).await
                });
            }
        }
        ForwardType::Device => {
            #[cfg(not(target_os = "windows"))]
            setup_device_point(ctx).await;
        }
        _ => {}
    }
}

pub fn filter_command(_payload: &[u8]) -> io::Result<(String, u32)> {
    let bytes = &_payload[4..];
    let ct: Result<String, std::string::FromUtf8Error> = String::from_utf8(bytes.to_vec());
    if let Ok(content) = ct {
        let mut id_bytes = [0u8; 4];
        id_bytes.copy_from_slice(&_payload[0..4]);
        let id: u32 = u32::from_be_bytes(id_bytes);
        return Ok((content, id));
    }
    Err(Error::new(ErrorKind::Other, "filter command failure"))
}

pub async fn dev_write_bufer(path: String, content: Vec<u8>) {
    utils::spawn(async move {
        let open_result = OpenOptions::new()
            .write(true)
            .create(true)
            .open(path.clone());

        match open_result {
            Ok(mut file) => {
                let write_result = file.write_all(content.as_slice());
                match write_result {
                    Ok(()) => {}
                    Err(e) => {
                        crate::error!("dev write bufer to file fail:{:#?}", e);
                    }
                }
            }
            Err(e) => {
                crate::error!("dev write bufer fail:{:#?}", e);
            }
        }
    });
}

pub async fn write_forward_bufer(ctx: &mut ContextForward, content: Vec<u8>) -> bool {
    if ctx.forward_type == ForwardType::Tcp {
        return TcpWriteStreamMap::write(ctx.id, content).await;
    } else if ctx.forward_type == ForwardType::Device {
        let path_ref = ctx.dev_path.clone();
        if path_ref.is_empty() {
            crate::error!(
                "write_forward_bufer get dev_path  is failed ctx.id = {:#?}",
                ctx.id
            );
            return false;
        }
        dev_write_bufer(path_ref, content).await;
    } else {
        #[cfg(not(target_os = "windows"))]
        {
            let fd = ctx.fd;
            if UdsClient::wrap_send(fd, &content) < 0 {
                crate::info!("write forward bufer failed. fd = {fd}");
                return false;
            }
        }
    }
    true
}

#[allow(unused)]
pub async fn malloc_context(
    session_id: u32,
    channel_id: u32,
    master_slave: bool,
) -> ContextForward {
    let mut ctx = ContextForward {
        ..Default::default()
    };
    ctx.id = utils::get_current_time() as u32;
    ctx.session_id = session_id;
    ctx.channel_id = channel_id;
    ctx.is_master = master_slave;
    crate::info!("malloc_context success id = {:#?}", ctx.id);
    ForwardContextMap::update(ctx.id, ctx.clone()).await;
    ctx
}

pub async fn forward_command_dispatch(
    session_id: u32,
    channel_id: u32,
    command: HdcCommand,
    _payload: &[u8],
) -> bool {
    let Some(task) = ForwardTaskMap::get(session_id, channel_id).await else {
        crate::error!("forward_command_dispatch get task is none session_id={session_id},channel_id={channel_id}"
        );
        return false;
    };
    let task: &mut HdcForward = &mut task.clone();
    let mut ret: bool = true;
    let cid = get_cid(_payload);
    let send_msg = _payload[4..].to_vec();

    let Some(context_forward) = ForwardContextMap::get(cid).await else {
        crate::error!("forward command dispatch get context is none cid={cid}");
        return false;
    };
    let ctx = &mut context_forward.clone();
    match command {
        HdcCommand::ForwardCheckResult => {
            ret = check_command(ctx, _payload, task.server_or_daemon).await;
        }
        HdcCommand::ForwardData => {
            ret = write_forward_bufer(ctx, send_msg).await;
        }
        HdcCommand::ForwardFreeContext => {
            free_context(ctx.id, false).await;
        }
        HdcCommand::ForwardActiveMaster => {
            read_data_to_forward(ctx).await;
            ret = true;
        }
        _ => {
            ret = false;
        }
    }
    ret
}

async fn print_error_info(session_id: u32, channel_id: u32) {
    let Some(task) = ForwardTaskMap::get(session_id, channel_id).await else {
        crate::error!(
            "detech_forward_type get task is none session_id = {:#?}, channel_id = {:#?}",
            session_id,
            channel_id
        );
        return;
    };
    let task = &mut task.clone();
    let ctx = &task.context_forward;
    let mut echo_content = ctx.last_error.clone();

    if echo_content.is_empty() {
        echo_content = "Forward parament failed".to_string();
    }

    hdctransfer::echo_client(
        session_id,
        channel_id,
        echo_content.as_str(),
        MessageLevel::Fail,
    )
    .await;
}

pub async fn command_dispatch(
    session_id: u32,
    channel_id: u32,
    command: HdcCommand,
    payload: &[u8],
    _payload_size: u16,
) -> bool {
    if command != HdcCommand::ForwardData {
        crate::info!("command_dispatch command recv: {:?}", command);
    }

    let ret = match command {
        HdcCommand::ForwardInit => begin_forward(session_id, channel_id, payload).await,
        HdcCommand::ForwardCheck => slave_connect(session_id, channel_id, payload, true).await,
        HdcCommand::ForwardActiveSlave => {
            slave_connect(session_id, channel_id, payload, false).await
        }
        _ => forward_command_dispatch(session_id, channel_id, command, payload).await,
    };
    if !ret {
        print_error_info(session_id, channel_id).await;
        task_finish(session_id, channel_id).await;
        return false;
    }
    ret
}
