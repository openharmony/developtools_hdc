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
//! serializer

pub mod native_struct;
pub mod pack_assemble;
pub mod pack_struct;
pub mod serialize;
pub mod unittest;

pub use pack_assemble::{concat_pack, unpack_payload_head, unpack_payload_protect};
pub use pack_struct::{HEAD_SIZE, UART_HEAD_SIZE, USB_HEAD_SIZE};
pub use serialize::buf_to_vec;
