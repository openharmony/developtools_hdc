/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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
//! context
#![allow(missing_docs)]
use std::collections::HashMap;
use std::mem::MaybeUninit;
use crate::utils::hdc_log::*;
use crate::common::forward;
use crate::common::hdcfile::FileTaskMap;
use crate::daemon_lib::daemon_app::AppTaskMap;
use crate::daemon_lib::shell::{ ShellExecuteMap, PtyMap };
use crate::config::ContextType;
use ylong_runtime::sync::Mutex;
use std::sync::Arc;
use std::sync::Once;

type ContextMap_ = Arc<Mutex<HashMap<(u32, u32), ContextType>>>;
pub struct ContextMap {}
impl ContextMap {
    pub(crate) fn get_instance() -> &'static ContextMap_ {
        static mut CONTEXT_MAP: MaybeUninit<ContextMap_> = MaybeUninit::uninit();
        static ONCE: Once = Once::new();
        unsafe {
            ONCE.call_once(|| {
                    CONTEXT_MAP = MaybeUninit::new(Arc::new(Mutex::new(HashMap::new())));
                }
            );
            &*CONTEXT_MAP.as_ptr()
        }
    }

    pub async fn put(session_id: u32, channel_id: u32, context_type: ContextType) {
        let arc_map = Self::get_instance();
        let mut map =  arc_map.lock().await; 
        map.insert((session_id, channel_id), context_type.clone());
    }

    pub async fn del(session_id: u32, channel_id: u32) {
        let arc_map = Self::get_instance();
        let mut map = arc_map.lock().await;
        map.remove(&(session_id, channel_id));
    }

    pub async fn channel_close(session_id: u32, channel_id: u32) {
        let context = {
            let arc_map = Self::get_instance();
            let mut map = arc_map.lock().await;
            let context = match map.get(&(session_id, channel_id)) {
                Some(context_type) => context_type.clone(),
                None => return,
            };
            crate::debug!(
                "remove task context_type: {:?}, session_id: {:?}, channel_id: {:?}",
                context,
                session_id,
                channel_id,
            );
            map.remove(&(session_id, channel_id));
            context
        };
        match context {
            ContextType::App => {
                AppTaskMap::remove(session_id, channel_id).await;
            }
            ContextType::File => {
                FileTaskMap::remove(session_id, channel_id).await;
            }
            ContextType::ExecuteShell => {
                ShellExecuteMap::del(session_id, channel_id).await;
            }
            ContextType::Shell => {
                if let Some(pty_task) = PtyMap::get(session_id, channel_id).await {
                    let _ = &pty_task.tx.send(vec![0x04_u8]).await;
                    PtyMap::del(session_id, channel_id).await;
                } else {
                    crate::error!("shell task is not exist");
                }
            }
            ContextType::Forward => {
                forward::free_channel_task(session_id, channel_id).await;
            }
            _ => {
                crate::debug!("unknown context is {:?}", context);
            }
        }
    }
}