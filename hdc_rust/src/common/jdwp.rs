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
#![allow(missing_docs)]
use crate::common::sendmsg::send_msg;
use crate::common::uds::{PollNode, UdsAddr, UdsServer};
use crate::config::ErrCode;
use crate::config::HdcCommand;
use crate::config::TaskMessage;
use crate::transfer;
use libc::{POLLERR, POLLHUP, POLLNVAL, POLLRDHUP, SOCK_STREAM};

use std::collections::HashMap;
use std::sync::Arc;
use ylong_runtime::sync::waiter::Waiter;
use ylong_runtime::sync::Mutex;

const JPID_SOCKET_PATH: &str = "ohjpid-control";
const PATH_LEN: usize = JPID_SOCKET_PATH.as_bytes().len() + 1;

type NodeMap = Arc<Mutex<HashMap<i32, PollNode>>>;
type Trackers = Arc<Mutex<Vec<(u32, u32, bool)>>>;

pub trait JdwpBase: Send + Sync + 'static {}
pub struct Jdwp {
    is_stopping: Arc<Mutex<bool>>,
    poll_node_map: NodeMap,
    empty_waiter: Arc<Waiter>,
    new_process_waiter: Arc<Waiter>,
    trackers: Trackers,
}

impl JdwpBase for Jdwp {}

type JdWpShare = Arc<Jdwp>;

impl Default for Jdwp {
    fn default() -> Self {
        Self::new()
    }
}

impl Jdwp {
    pub fn get_instance() -> JdWpShare {
        static mut INSTANCE: Option<JdWpShare> = None;
        unsafe {
            INSTANCE
                .get_or_insert_with(|| Arc::new(Jdwp::new()))
                .clone()
        }
    }

    pub fn new() -> Self {
        Self {
            is_stopping: Arc::new(Mutex::new(false)),
            poll_node_map: Arc::new(Mutex::new(HashMap::default())),
            empty_waiter: Arc::new(Waiter::new()),
            new_process_waiter: Arc::new(Waiter::new()),
            trackers: Arc::new(Mutex::new(Vec::new())),
        }
    }
}

impl Jdwp {
    pub async fn send_fd_to_target(&self, target_pid: u32, fd: i32, parameter: &str) -> bool {
        let map = self.poll_node_map.clone();
        let map = map.lock().await;
        let keys = map.keys();
        for k in keys {
            let v = map.get(k);
            if let Some(node) = v {
                if node.ppid == target_pid {
                    let bytes = fd.to_be_bytes();
                    let fd_bytes = bytes.as_slice();
                    let param_bytes = parameter.as_bytes();
                    let param_bytes = [fd_bytes, param_bytes].concat();
                    let param_bytes = param_bytes.as_slice();
                    let ret = send_msg(node.fd, fd, param_bytes);
                    println!("send_fd_to_target ret:{}", ret);
                    return ret > 0;
                }
            }
        }
        false
    }

    async fn send_process_list(trackers: Trackers, node_map: NodeMap) {
        let trackers = trackers.lock().await;
        for (channel_id2, session_id2, is_debug) in trackers.iter() {
            let message = Self::get_process_list_with_pkg_name(node_map.clone(), *is_debug).await;
            let len = message.as_bytes().len();
            let len_str = format!("{:04x}\n", len);
            let mut header = len_str.as_bytes().to_vec();
            let mut buffer = Vec::<u8>::new();
            buffer.append(&mut header);
            buffer.append(&mut message.as_str().as_bytes().to_vec());

            let data = TaskMessage {
                channel_id: *channel_id2,
                command: HdcCommand::KernelEchoRaw,
                payload: buffer.to_vec(),
            };
            transfer::put(*session_id2, data).await;
        }
    }

    pub async fn add_tracker(&self, channel_id: u32, session_id: u32, debug_or_release: bool) {
        let mut trackers_lock = self.trackers.lock().await;
        trackers_lock.push((channel_id, session_id, debug_or_release));
        drop(trackers_lock);

        let node_map = self.poll_node_map.clone();
        Self::send_process_list(self.trackers.clone(), node_map).await;
    }

    pub async fn get_process_list(&self) -> String {
        let mut result = String::from("");
        let map = self.poll_node_map.clone();
        let map = map.lock().await;
        let keys = map.keys();
        for key in keys {
            let value = map.get(key);
            if let Some(v) = value {
                result.push_str((v.ppid.to_string() + "\n").as_str());
            }
        }
        result
    }

    pub async fn get_process_list_with_pkg_name(map: NodeMap, debug_or_release: bool) -> String {
        let mut result = String::from("");
        let map = map.lock().await;
        let keys = map.keys();
        for key in keys {
            let value = map.get(key);
            if let Some(v) = value {
                if !debug_or_release || debug_or_release == v.debug_or_release {
                    result
                        .push_str((v.ppid.to_string() + " " + v.pkg_name.as_str() + "\n").as_str());
                }
            }
        }
        result
    }

    pub async fn handle_client(
        fd: i32,
        waiter: Arc<Waiter>,
        node_map: NodeMap,
        trackers: Trackers,
    ) {
        println!("handle_client start...");
        loop {
            let mut buffer: [u8; 1024] = [0; 1024];
            let size = UdsServer::wrap_recv(fd, &mut buffer);
            let u32_size = std::mem::size_of::<u32>();
            if size == u32_size.try_into().unwrap() {
                let _pid = u32::from_le_bytes(buffer[0..u32_size].try_into().unwrap());
            } else if size > u32_size.try_into().unwrap() {
                let len = u32::from_le_bytes(buffer[0..u32_size].try_into().unwrap());
                let pid = u32::from_le_bytes(buffer[u32_size..2 * u32_size].try_into().unwrap());
                println!("pid:{}", pid);
                let debug_or_release =
                    u32::from_le_bytes(buffer[u32_size * 2..3 * u32_size].try_into().unwrap()) == 1;
                println!("debug:{}", debug_or_release);
                let pkg_name =
                    String::from_utf8(buffer[u32_size * 3..len as usize].to_vec()).unwrap();
                println!("pkg name:{}", pkg_name);

                let node_map = node_map.clone();
                let mut map = node_map.lock().await;
                let node = PollNode::new(fd, pid, pkg_name.clone(), debug_or_release);
                let mut key_ = -1;
                for (key, value) in map.iter() {
                    if value.pkg_name == pkg_name {
                        key_ = *key;
                        UdsServer::wrap_close(value.fd);
                        break;
                    }
                }
                map.remove(&key_);
                map.insert(fd, node);
                drop(map);

                let trackers = trackers.clone();
                let node_map = node_map.clone();
                Self::send_process_list(trackers, node_map).await;

                waiter.wake_one();
            } else if size <= 0 {
                println!("size <= 0");
                break;
            }
        }
    }

    pub fn jdwp_listen(&self) -> bool {
        let fd = UdsServer::wrap_socket(SOCK_STREAM);
        let name = JPID_SOCKET_PATH.as_bytes();
        let socket_name = &mut [0u8; PATH_LEN];
        socket_name[0] = b'\0';
        socket_name[1..].copy_from_slice(name);
        let addr = UdsAddr::parse_abstract(&socket_name[1..]);
        if let Ok(addr_obj) = &addr {
            let ret = UdsServer::wrap_bind(fd, addr_obj);
            if ret.is_err() {
                println!("bind fail");
                return false;
            }
            let ret = UdsServer::wrap_listen(fd);
            if ret < 0 {
                println!("listen fail");
                return false;
            }
            let node_map = self.poll_node_map.clone();
            let trackers = self.trackers.clone();
            let stop = self.is_stopping.clone();
            let waiter = self.new_process_waiter.clone();
            ylong_runtime::spawn(async move {
                loop {
                    let stop_flag = stop.lock().await;
                    if *stop_flag {
                        return;
                    }

                    drop(stop_flag);
                    let client_fd = UdsServer::wrap_accept(fd);
                    if client_fd == -1 {
                        break;
                    }
                    let map = node_map.clone();
                    let trackers = trackers.clone();
                    let w = waiter.clone();
                    ylong_runtime::spawn(Self::handle_client(client_fd, w, map, trackers));
                }
            });
            true
        } else {
            println!("parse addr fail  ");
            false
        }
    }

    pub fn start_data_looper(&self) {
        let node_map = self.poll_node_map.clone();
        let waiter = self.empty_waiter.clone();
        let stop = self.is_stopping.clone();
        let trackers = self.trackers.clone();
        ylong_runtime::spawn(async move {
            loop {
                let stop_flag = stop.lock().await;
                if *stop_flag {
                    return;
                }
                drop(stop_flag);
                let mut poll_nodes = Vec::<PollNode>::new();
                let mut size = poll_nodes.len();
                let node_map_value = node_map.lock().await;
                if node_map_value.is_empty() {
                    let w = waiter.clone();
                    drop(node_map_value);
                    println!("start_data_looper, empty_waiter wait...");
                    w.wait().await;
                    println!("start_data_looper, empty_waiter wait continue...");
                    continue;
                }
                let keys = node_map_value.keys();
                for k in keys {
                    if let Some(n) = node_map_value.get(k) {
                        poll_nodes.push(n.clone());
                        size = poll_nodes.len();
                    }
                }
                if poll_nodes.is_empty() {
                    continue;
                }
                for pnode in &poll_nodes {
                    println!(
                        "before poll, node:{},{},{},{}",
                        pnode.fd, pnode.events, pnode.revents, pnode.ppid
                    );
                }
                drop(node_map_value);
                UdsServer::wrap_poll(poll_nodes.as_mut_slice(), size.try_into().unwrap(), -1);
                let mut node_map_value = node_map.lock().await;
                for pnode in &poll_nodes {
                    println!(
                        "after poll, node:{},{},{},{}",
                        pnode.fd, pnode.events, pnode.revents, pnode.ppid
                    );

                    if pnode.revents & (POLLNVAL | POLLRDHUP | POLLHUP | POLLERR) != 0 {
                        node_map_value.remove(&pnode.fd);
                        UdsServer::wrap_close(pnode.fd);
                        break;
                    }
                }
                drop(node_map_value);
                let trackers = trackers.clone();
                let node_map = node_map.clone();
                Self::send_process_list(trackers, node_map).await;
            }
        });
    }

    pub async fn create_fd_event_poll(&self) {
        loop {
            let is_stopping = self.is_stopping.clone();
            let stop_flag = is_stopping.lock().await;
            if *stop_flag {
                return;
            }

            drop(stop_flag);
            let waiter = self.new_process_waiter.clone();
            waiter.wait().await;

            let node_map = self.poll_node_map.clone();
            let node_map_value = node_map.lock().await;
            if !node_map_value.is_empty() {
                let empty_waiter = self.empty_waiter.clone();
                empty_waiter.wake_one();
            }
        }
    }

    pub async fn init(&self) -> ErrCode {
        println!("jdwp init....");

        if !self.jdwp_listen() {
            println!("jdwp_listen failed");
            return ErrCode::ModuleJdwpFailed;
        }

        self.start_data_looper();

        self.create_fd_event_poll().await;
        ErrCode::Success
    }
}
