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
//! uart_wrapper
#![allow(missing_docs)]
use super::uart::UartWriter;
use super::{uart, UartMap};
use crate::config::{self, TaskMessage};
use crate::serializer::native_struct::UartHead;
use crate::serializer::serialize::Serialization;
use crate::serializer::{self, UART_HEAD_SIZE};
#[allow(unused)]
use crate::utils::hdc_log::*;
use std::collections::HashMap;
use std::sync::Arc;
use ylong_runtime::sync::waiter::Waiter;
use ylong_runtime::sync::Mutex;
use ylong_runtime::task::JoinHandle;

#[derive(PartialEq, Debug, Clone, Copy)]
#[repr(u8)]
pub enum UartOption {
    Tail = 1,  // makr is the last packget, can be send to session.
    Reset = 2, // host request reset session in daemon
    Ack = 4,   // response the pkg is received
    Nak = 8,   // request resend pkg again
    Free = 16, // request free this session, some unable recovery error happened
}

impl TryFrom<u8> for UartOption {
    type Error = ();
    fn try_from(cmd: u8) -> Result<Self, ()> {
        match cmd {
            1 => Ok(Self::Tail),
            2 => Ok(Self::Reset),
            4 => Ok(Self::Ack),
            8 => Ok(Self::Nak),
            16 => Ok(Self::Free),
            _ => Err(()),
        }
    }
}

struct WaiterManager {
    // waiter used for sync package send-response one by one.
    response_waiters: HashMap<u32, Waiter>,
    // waiter used for waiting if no packages.
    empty_waiters: HashMap<u32, Waiter>,
}

impl WaiterManager {
    fn get_instance() -> &'static mut WaiterManager {
        static mut INSTANCE: Option<WaiterManager> = None;
        unsafe {
            INSTANCE.get_or_insert(WaiterManager {
                response_waiters: HashMap::new(),
                empty_waiters: HashMap::new(),
            })
        }
    }

    async fn start_session(session_id: u32) {
        let instance = Self::get_instance();
        instance.response_waiters.insert(session_id, Waiter::new());
        instance.empty_waiters.insert(session_id, Waiter::new());
    }

    #[allow(unused)]
    async fn wait_response(session_id: u32) {
        let instance = Self::get_instance();
        let waiter = instance.response_waiters.get(&session_id);
        if let Some(w) = waiter {
            w.wait().await;
        }
    }

    #[allow(unused)]
    async fn wakeup_response_wait(session_id: u32) {
        let instance = Self::get_instance();
        let waiter = instance.response_waiters.get(&session_id);
        if let Some(w) = waiter {
            w.wake_one();
        }
    }

    #[allow(unused)]
    async fn wait_empty(session_id: u32) {
        let instance = Self::get_instance();
        let waiter = instance.empty_waiters.get(&session_id);
        if let Some(w) = waiter {
            w.wait().await;
        }
    }

    #[allow(unused)]
    async fn wakeup_empty_wait(session_id: u32) {
        let instance = Self::get_instance();
        let waiter = instance.empty_waiters.get(&session_id);
        if let Some(w) = waiter {
            w.wake_one();
        }
    }
}

#[derive(PartialEq, Debug, Clone, Copy)]
#[repr(u8)]
enum OutputDataStatus {
    WaitSend = 0,
    WaitResponse = 1,
    ResponseOk = 2,
}

#[derive(PartialEq, Debug, Clone)]
struct OutputData {
    session_id: u32,
    response: bool,
    option: u8,
    package_index: u32,
    data: Vec<u8>,
    status: OutputDataStatus,
    retry_count: u32,
}

impl std::fmt::Display for OutputData {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "OutputData: session_id:{}, response:{}, option:{:#?}, package_index:{}, status:{:#?}, retry_count:{}, data size:{}", 
        self.session_id, self.response, self.option, self.package_index, self.status, self.retry_count, self.data.len())
    }
}

type OutputData_ = Arc<Mutex<OutputData>>;

type OutputDataVec_ = Arc<Mutex<Vec<OutputData_>>>;

struct DataQueue {
    data_map: HashMap<u32, OutputDataVec_>,
    thread_map: HashMap<u32, JoinHandle<()>>,
    stop_flag_map: HashMap<u32, Arc<Mutex<u8>>>,
}

impl DataQueue {
    fn new() -> Self {
        Self {
            data_map: HashMap::new(),
            thread_map: HashMap::new(),
            stop_flag_map: HashMap::new(),
        }
    }
}

type DataQueue_ = Arc<Mutex<DataQueue>>;

pub struct QueueManager {}

impl QueueManager {
    fn get_instance() -> DataQueue_ {
        static mut INSTANCE: Option<DataQueue_> = None;
        unsafe {
            INSTANCE
                .get_or_insert_with(|| Arc::new(Mutex::new(DataQueue::new())))
                .clone()
        }
    }


    async fn get_package(session_id: u32, index: usize) -> Option<OutputData> {
        let instance = Self::get_instance();
        let mtx = instance.lock().await;
        let data_map = &mtx.data_map;
        if let Some(vec) = data_map.get(&session_id) {
            let vec = vec.lock().await;
            if !vec.is_empty() {
                let arc = vec.get(index).unwrap();
                let data_mtx = arc.lock().await;
                return Some(data_mtx.clone());
            }
        }
        None
    }

    async fn put_package(session_id: u32, data: OutputData) {
        let instance = Self::get_instance();
        let mut mtx = instance.lock().await;
        let data_map = &mut mtx.data_map;
        if let Some(vec) = data_map.get(&session_id) {
            let mut vec = vec.lock().await;
            let item = Arc::new(Mutex::new(data));
            vec.push(item);
        } else {
            let mut vec = Vec::<Arc<Mutex<OutputData>>>::new();
            let d = Arc::new(Mutex::new(data));
            vec.push(d);
            let v = Arc::new(Mutex::new(vec));
            data_map.insert(session_id, v);
        }
    }

    async fn update_package(session_id: u32, index: usize, data: OutputData) -> bool {
        let instance = Self::get_instance();
        let mtx = instance.lock().await;
        let data_map = &mtx.data_map;
        if let Some(vec) = data_map.get(&session_id) {
            let vec = vec.lock().await;
            if !vec.is_empty() {
                let arc = vec.get(index).unwrap();
                let mut data_mtx = arc.lock().await;
                *data_mtx = data;
                return true;
            }
        }
        false
    }

    async fn get_stop_flag(session_id: u32) -> Option<u8> {
        let instance = Self::get_instance();
        let mtx = instance.lock().await;
        let stop_flag_map = &mtx.stop_flag_map;
        if let Some(flag) = stop_flag_map.get(&session_id) {
            let v = flag.lock().await;
            Some(*v)
        } else {
            None
        }
    }

    #[allow(unused)]
    async fn set_stop_flag(session_id: u32) {
        let instance = Self::get_instance();
        let mut mtx = instance.lock().await;
        let stop_flag_map = &mut mtx.stop_flag_map;
        stop_flag_map.insert(session_id, Arc::new(Mutex::new(1)));
    }

    async fn remove_package(session_id: u32, index: usize) -> bool {
        let instance = Self::get_instance();
        let mtx = instance.lock().await;
        let data_map = &mtx.data_map;
        if let Some(vec) = data_map.get(&session_id) {
            let mut vec = vec.lock().await;
            if !vec.is_empty() && index < vec.len() {
                vec.remove(index);
                return true;
            }
        }
        false
    }

    async fn remove_session(session_id: u32) {
        let instance = Self::get_instance();
        let mut mtx = instance.lock().await;
        mtx.data_map.remove(&session_id);
        mtx.stop_flag_map.remove(&session_id);
        mtx.thread_map.remove(&session_id);
        println!("remove_session:{session_id}");
    }

    async fn check_stop(session_id: u32) -> bool {
        if let Some(stop) = Self::get_stop_flag(session_id).await {
            return stop == 0;
        }
        false
    }

    async fn session_loop(session_id: u32) {
        // 1. 取第[0]个outputdata, 如果是WaitSend 则发送 改变状态为WaitResponse 同时wait
        //   2. 收到response, 如果是ACK 则改变为ResponseOK 同时wakeup
        //   3.收到wakeup,则检查状态是否为ResponseOK 如果是，则remove掉，继续step 1;
        //      如果不是，则检查retry_count, 自减1，继续send， 同时继续超时wait(如果超时，则继续检查状态，retry count 减1，继续send, 超时wait)
        //      retry count为0， 则表示连接中断，stop session
        println!("session_loop for {}", session_id);
        loop {
            if Self::check_stop(session_id).await {
                break;
            }
            let mut first_pkg = Self::get_package(session_id, 0).await;
            while first_pkg.is_none() {
                WaiterManager::wait_empty(session_id).await;
                first_pkg = Self::get_package(session_id, 0).await;
                if Self::check_stop(session_id).await {
                    break;
                }
            }
            if Self::check_stop(session_id).await {
                break;
            }
            let mut first_pkg = first_pkg.unwrap();
            let mut status = first_pkg.status;
            let mut retry_count = first_pkg.retry_count;

            if status == OutputDataStatus::WaitSend {
                // 发送数据
                let data = first_pkg.data.clone();
                let _ret = UartMap::put(session_id, data).await;
                // 如果是ack报文 则不需要等待回应
                if first_pkg.response {
                    QueueManager::remove_package(session_id, 0).await;
                    continue;
                }
                // 修改data 的status = WaitResponse
                first_pkg.status = OutputDataStatus::WaitResponse;
                retry_count -= 1;
                first_pkg.retry_count = retry_count;
                // 更新数据
                QueueManager::update_package(session_id, 0, first_pkg.clone()).await;
                // 等待response
                WaiterManager::wait_response(session_id).await;

                if Self::check_stop(session_id).await {
                    break;
                }
                // 收到回复
                // 重新获取数据

                let first_pkg = Self::get_package(session_id, 0).await;

                let mut first_pkg = first_pkg.unwrap();
                // 得到新状态
                status = first_pkg.status;

                if status == OutputDataStatus::ResponseOk {
                    // 删除当前data
                    QueueManager::remove_package(session_id, 0).await;
                    continue;
                }
                retry_count = first_pkg.retry_count;
                while retry_count > 0 && status == OutputDataStatus::WaitResponse {
                    // 保存retry_count
                    retry_count -= 1;
                    first_pkg.retry_count = retry_count;
                    QueueManager::update_package(session_id, 0, first_pkg.clone()).await;

                    // 再次发送数据
                    let data = first_pkg.data.clone();
                    let _ret = UartMap::put(session_id, data).await;
                    WaiterManager::wait_response(session_id).await;

                    if Self::check_stop(session_id).await {
                        break;
                    }

                    let first_pkg = Self::get_package(session_id, 0).await;

                    let first_pkg = first_pkg.unwrap();
                    status = first_pkg.status;

                    match status {
                        OutputDataStatus::ResponseOk => {
                            QueueManager::remove_package(session_id, 0).await;
                            break;
                        }
                        OutputDataStatus::WaitResponse => {
                            let first_pkg = Self::get_package(session_id, 0).await;
                            let first_pkg = first_pkg.unwrap();
                            status = first_pkg.status;
                            retry_count = first_pkg.retry_count;
                            continue;
                        }
                        OutputDataStatus::WaitSend => {
                            QueueManager::remove_package(session_id, 0).await;
                            break;
                        }
                    }
                }
            }
        }
        Self::remove_session(session_id).await;
        println!("session_loop for {} end.", session_id);
    }
}

pub async fn start_session(session_id: u32) {
    let instance = QueueManager::get_instance();
    let mut mtx = instance.lock().await;
    let thread_map = &mut mtx.thread_map;
    if thread_map.contains_key(&session_id) {
        println!("session thread has started.");
        return;
    }

    WaiterManager::start_session(session_id).await;

    let handle = ylong_runtime::spawn(QueueManager::session_loop(session_id));
    thread_map.insert(session_id, handle);

    let stop_flag_map = &mut mtx.stop_flag_map;
    stop_flag_map.insert(session_id, Arc::new(Mutex::new(1)));
}

async fn stop_session(session_id: u32) {
    let instance = QueueManager::get_instance();
    let mut mtx = instance.lock().await;
    let stop_flag_map = &mut mtx.stop_flag_map;
    stop_flag_map.insert(session_id, Arc::new(Mutex::new(0)));

    WaiterManager::wakeup_empty_wait(session_id).await;
    WaiterManager::wakeup_response_wait(session_id).await;
}

pub async fn stop_other_session(session_id: u32) {
    let instance = QueueManager::get_instance();
    let mtx = instance.lock().await;
    let session_ids = mtx.data_map.keys();
    let mut remove_sessions = Vec::new();
    for k in session_ids {
        if *k != session_id {
            remove_sessions.push(*k);
        }
    }
    drop(mtx);
    for id in remove_sessions {
        stop_session(id).await;
    }
}

async fn output_package(
    session_id: u32,
    response: bool,
    option: u8,
    package_index: u32,
    data: Vec<u8>,
) {
    let pkg = OutputData {
        session_id,
        response,
        option,
        package_index,
        data: data.clone(),
        retry_count: 5,
        status: OutputDataStatus::WaitSend,
    };
    QueueManager::put_package(session_id, pkg).await;
    WaiterManager::wakeup_empty_wait(session_id).await;
}

#[allow(unused)]
fn is_response(option: u8) -> bool {
    let ret = (option & UartOption::Ack as u8) | (option & UartOption::Nak as u8);
    ret != 0
}

pub async fn on_read_head(head: UartHead) {
    let session_id = head.session_id;
    let option = head.option;
    let package_index = head.package_index;
    if option & (UartOption::Free as u16) != 0 {
        stop_session(session_id).await;
        return;
    }
    if is_response(option as u8) {
        let pkg = QueueManager::get_package(session_id, 0).await;
        let mut pkg = pkg.unwrap();
        pkg.status = if option & (UartOption::Ack as u16) > 1 {
            OutputDataStatus::ResponseOk
        } else {
            OutputDataStatus::WaitSend
        };
        QueueManager::update_package(session_id, 0, pkg).await;
        WaiterManager::wakeup_response_wait(session_id).await;
    } else {
        let mut header_obj =
            uart::build_header_obj(session_id, UartOption::Ack as u16, 0, package_index);
        let header = header_obj.serialize();
        let head_sum = header.iter().fold(0u32, |acc, &x| acc + x as u32);
        header_obj.head_checksum = u32::to_le(head_sum);
        let data = header_obj.serialize();
        output_package(session_id, true, UartOption::Ack as u8, package_index, data).await;
    }
}

#[allow(unused)]
fn get_package_index(is_create: bool) -> u32 {
    static mut PACKAGE_INDEX: u32 = 888;

    unsafe {
        if is_create {
            PACKAGE_INDEX += 1;
            PACKAGE_INDEX
        } else {
            PACKAGE_INDEX
        }
    }
}

pub async fn start_uart(session_id: u32, wr: UartWriter) {
    UartMap::start(session_id, wr).await;
}

#[allow(unused)]
pub async fn wrap_put(session_id: u32, data: TaskMessage, package_index: u32, option: u8) {
    let mut pkg_index = package_index;
    if package_index == 0 {
        pkg_index = get_package_index(true);
    }
    let send = serializer::concat_pack(data);
    crate::info!("wrap_put send len:{}, send:{:#?}", send.len(), send);

    let payload_max_len = config::MAX_UART_SIZE_IOBUF as usize - UART_HEAD_SIZE;
    let mut index = 0;
    let len = send.len();

    loop {
        if index >= len {
            println!("wrap_put break");
            break;
        }
        let size;
        let mut op = option;
        if index + payload_max_len <= len {
            size = payload_max_len;
        } else {
            size = len - index;
            op = UartOption::Tail as u8 | option;
        }

        let data = send[index..index + size].to_vec().clone();
        let data_sum = data.iter().fold(0u32, |acc, &x| acc + x as u32);
        let mut header_obj = uart::build_header_obj(session_id, op as u16, size, pkg_index);
        header_obj.data_checksum = u32::to_le(data_sum);

        let header = header_obj.serialize();
        let head_sum = header.iter().fold(0u32, |acc, &x| acc + x as u32);
        header_obj.head_checksum = u32::to_le(head_sum);

        let header = header_obj.serialize();
        crate::info!("header, header_len:{}", header.len());
        let total = [header, send[index..index + size].to_vec().clone()].concat();

        output_package(
            session_id,
            (op & UartOption::Ack as u8) > 0,
            op,
            pkg_index,
            total,
        )
        .await;
        pkg_index = get_package_index(true);
        index += size;
    }
}
