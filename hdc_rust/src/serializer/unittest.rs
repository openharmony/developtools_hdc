/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
//! unittest
#![allow(missing_docs)]

#[cfg(test)]
mod tests {
    use crate::serializer::native_struct;
    use crate::serializer::serialize::Serialization;

    #[test]
    fn if_session_hand_shake_works() {
        let send = native_struct::SessionHandShake {
            banner: "test_banner".to_string(),
            auth_type: 1,
            session_id: 2,
            connect_key: "test_connect_key".to_string(),
            buf: "test_buf".to_string(),
            version: "test_version".to_string(),
        };

        let serialized = send.serialize();
        let mut recv = native_struct::SessionHandShake::default();
        let suc = recv.parse(serialized);

        println!("{:#?}", recv);
        assert!(suc.is_ok());
        assert_eq!(recv, send);
    }

    #[test]
    fn if_session_transfer_payload_works() {
        let send = native_struct::TransferPayload {
            index: 1 << 60,
            compress_type: 1 << 6,
            compress_size: 1 << 20,
            uncompress_size: 1 << 23,
        };

        let serialized = send.serialize();
        let mut recv = native_struct::TransferPayload::default();
        let suc = recv.parse(serialized);

        println!("{:#?}", recv);
        assert!(suc.is_ok());
        assert_eq!(recv, send);
    }

    #[test]
    fn if_transfer_config_works() {
        let send = native_struct::TransferConfig {
            file_size: 1 << 40,
            atime: 1 << 50,
            mtime: 1 << 60,
            options: "options".to_string(),
            path: "path".to_string(),
            optional_name: "optional_name".to_string(),
            update_if_new: true,
            compress_type: 3,
            hold_timestamp: false,
            function_name: "function_name".to_string(),
            client_cwd: "client_cwd\\client_cwd".to_string(),
            reserve1: "reserve1".to_string(),
            reserve2: "reserve2".to_string(),
        };

        let serialized = send.serialize();
        let mut recv = native_struct::TransferConfig {
            ..Default::default()
        };
        let suc = recv.parse(serialized);

        println!("{:#?}", recv);
        assert!(suc.is_ok());
        assert_eq!(recv, send);
    }

    #[test]
    fn if_session_payload_head_works() {
        let send = native_struct::PayloadHead {
            flag: [1, 2],
            reserve: [3, 4],
            protocol_ver: 0x11,
            head_size: 0x22,
            data_size: 0x33,
        };
        let serialized = send.serialize();
        let mut recv = native_struct::PayloadHead::default();
        let suc = recv.parse(serialized);

        println!("{:#?}", recv);
        assert!(suc.is_ok());
        assert_eq!(recv, send);
    }
}
