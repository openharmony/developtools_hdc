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
//! taskbase
#![allow(missing_docs)]

use crate::config::HdcCommand;

pub trait TaskBase: Send + Sync + 'static {
    fn command_dispatch(
        &mut self,
        _command: HdcCommand,
        _payload: &[u8],
        _payload_size: u16,
    ) -> bool;
    fn stop_task(&mut self);
    fn ready_for_release(&mut self) -> bool;
    fn channel_id(&self) -> u32 {
        0
    }
    fn task_finish(&self);
}
