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
//! shell

#[allow(unused_imports)]
use crate::daemon_lib::daemon_app;
use crate::daemon_lib::shell;
use crate::daemon_lib::auth;
#[allow(unused_imports)]
use crate::common::forward;
#[allow(unused_imports)]
use crate::common::jdwp;
#[allow(unused_imports)]
use crate::common::hdcfile;
#[allow(unused_imports)]
use crate::utils::hdc_log::*;
use crate::config::ConnectType;
use crate::transfer::buffer;
use crate::transfer::TcpMap;
use crate::transfer::UsbMap;
use crate::transfer::ConnectTypeMap;

pub async fn free_all_sessiones() {
    let sessiones = ConnectTypeMap::get_all_session().await;
    for session_id in sessiones {
        free_session(session_id).await;
    }
}

pub async fn free_session(session_id: u32) {
    auth::AuthStatusMap::remove(session_id).await;
    stop_task(session_id).await;
    match ConnectTypeMap::get(session_id).await {
        Some(ConnectType::Bt) => {}
        Some(ConnectType::Tcp) => {
            TcpMap::end(session_id).await;
        }
        Some(ConnectType::Uart) => {}
        Some(ConnectType::Usb(_)) => {
            UsbMap::end(session_id).await;
        }

        Some(ConnectType::HostUsb(_)) => {
            // add to avoid warning
        }

        Some(ConnectType::Bridge) => {}

        None => {
            crate::warn!("free_session cannot find connect type for session_id:{session_id}");
        }
    }
}

pub async fn stop_task(session_id: u32) {
    hdcfile::stop_task(session_id).await;
    shell::stop_task(session_id).await;
    daemon_app::stop_task(session_id).await;
    forward::stop_task(session_id).await;
    jdwp::stop_session_task(session_id).await;
}

pub async fn dump_running_task_info() -> String {
    let mut result = "\n".to_string();
    result.push_str(&format!("{:#}", buffer::dump_session().await));
    result.push_str(&format!("{:#}", hdcfile::dump_task().await));
    result.push_str(&format!("{:#}", shell::dump_task().await));
    result.push_str(&format!("{:#}", daemon_app::dump_task().await));
    result.push_str(&format!("{:#}", forward::dump_task().await));
    result.push_str("# ");
    result.to_string()
}
