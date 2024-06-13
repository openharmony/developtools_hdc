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
//! command_queue
#![allow(missing_docs)]

use ylong_runtime::sync::Waiter;
use std::collections::VecDeque;
use std::sync::Arc;
use crate::config::{TaskMessage, HdcCommand};
use ylong_runtime::sync::Mutex;
#[allow(unused)]
use crate::utils::hdc_log::*;

const MAX_ONCE_WRITE_HEAVY: usize = 10;

struct TaskMessageExt {
    data: TaskMessage,
    session_id: u32,
}

struct BlockVecDeque {
    waiter: Waiter,
    full_waiter: Waiter,
    queue: Mutex<VecDeque<TaskMessageExt>>,
    light_queue: Mutex<VecDeque<TaskMessageExt>>,
    running: Mutex<bool>,
}

impl BlockVecDeque {
    fn new() -> Self {
        Self {
            waiter: Waiter::new(),
            full_waiter: Waiter::new(),
            queue: Mutex::new(VecDeque::new()),
            light_queue: Mutex::new(VecDeque::new()),
            running: Mutex::new(true),
        }
    }

    fn is_light_command(&self, cmd: HdcCommand) -> bool {
        let value = cmd as u32;
        let min = HdcCommand::FileInit as u32;
        let max = HdcCommand::FileRecvInit as u32;
        value < min || value > max
    }

    async fn push_back(&self, data: TaskMessage, session_id: u32) {
        let cmd = data.command;
        let mut queue;
        if self.is_light_command(cmd) {
            queue = self.light_queue.lock().await;
        } else {
            queue = self.queue.lock().await;
            if queue.len() >= MAX_ONCE_WRITE_HEAVY {
                drop(queue);
                self.full_waiter.wait().await;
                if !self.is_running().await {
                    return;
                }
                queue = self.queue.lock().await;
            }
        }
        queue.push_back(TaskMessageExt {
            data,
            session_id
        });
        self.waiter.wake_one();
    }

    async fn clear(&self) {
        let mut queue = self.queue.lock().await;
        queue.clear();
    }

    async fn pop_front(&self) -> Option<Vec<TaskMessageExt>> {
        if !self.is_running().await {
            return None;
        }
        let mut queue = self.queue.lock().await;
        if queue.is_empty() {
            drop(queue);
            self.waiter.wait().await;
            if !self.is_running().await {
                return None;
            }
            queue = self.queue.lock().await;
        }
        let mut result = Vec::new();
        loop {
            let message = queue.pop_front();
            let mut count = 0;
            if let Some(task_message) = message {
                let command = task_message.data.command;
                result.push(task_message);
                if command == HdcCommand::FileData {
                    count += 1;
                    if count >= MAX_ONCE_WRITE_HEAVY {
                        break;
                    }
                }
            } else {
                break;
            }
        }

        let len = queue.len();
        if len < MAX_ONCE_WRITE_HEAVY {
            self.full_waiter.wake_one();        
        }
        Some(result)
    }

    async fn pop_front_light(&self) -> Option<Vec<TaskMessageExt>> {
        if !self.is_running().await {
            return None;
        }
        let mut queue = self.light_queue.lock().await;
        if queue.is_empty() {
            return None;
        }
        let queue_len = queue.len();
        const MAX_LIGHT_COUNT_ONCE: usize = 100;
        let len = if queue_len > MAX_LIGHT_COUNT_ONCE {
            MAX_LIGHT_COUNT_ONCE
        } else {
            queue_len
        };
        let mut result = Vec::new();
        for _i in 0..len {
            if let Some(task_message) = queue.pop_front() {
                result.push(task_message);
            }
        }
        Some(result)
    }

    async fn set_running(&self, running: bool) {
        let mut running_lock = self.running.lock().await;
        *running_lock = running;
        self.waiter.wake_one();
        self.full_waiter.wake_all();
    }

    async fn is_running(&self) -> bool {
        let lock = self.running.lock().await;
        *lock
    }
}

type UsbPacketQueue_ = Arc<BlockVecDeque>;
pub struct UsbPacketQueue {}
impl UsbPacketQueue {
    fn get_instance() -> UsbPacketQueue_ {
        static mut USB_PACKET_QUEUE: Option<UsbPacketQueue_> = None;
        unsafe {
            USB_PACKET_QUEUE
                .get_or_insert_with(|| Arc::new(BlockVecDeque::new()))
                .clone()
        }
    }

    pub async fn push(session_id: u32, data: TaskMessage) {
        let instance = Self::get_instance();
        instance.push_back(data, session_id).await;
    }

    pub async fn pop() -> Option<Vec<(u32, TaskMessage)>> {
        let instance = Self::get_instance();
        let instance1 = instance.clone();
        if let Some(task_message_ext) = instance1.pop_front().await {
            let mut result = Vec::new();
            for item in task_message_ext {
                result.push((item.session_id, item.data));
            }
            Some(result)
        } else {
            None
        }
    }

    pub async fn pop_light() -> Option<Vec<(u32, TaskMessage)>> {
        let instance = Self::get_instance();
        let instance1 = instance.clone();
        if let Some(task_message_ext) = instance1.pop_front_light().await {
            let mut result = Vec::new();
            for item in task_message_ext {
                result.push((item.session_id, item.data));
            }
            Some(result)
        } else {
            None
        }
    }

    pub async fn is_running() -> bool {
        let instance = Self::get_instance();
        instance.is_running().await
    }

    pub async fn set_running(running: bool) {
        let instance = Self::get_instance();
        instance.set_running(running).await;
    }

    pub async fn clear() {
        let instance = Self::get_instance();
        instance.clear().await;
    }
}