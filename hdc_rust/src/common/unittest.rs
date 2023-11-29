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
//! unittest
#![allow(missing_docs)]

/// #[cfg(test)]
/// mod session_test {
///     use ylong_runtime;
///
///     use crate::common::hsession;
///     use crate::common::hsession::{ActionType, HdcSession};
///     use crate::config::{ConnectType, NodeType};
///     use std::time::Duration;
///
///     #[ylong_runtime::test]
///     async fn if_hsession_query_work() {
///         let t1 = ylong_runtime::spawn(async {
///             let hs = HdcSession::new(
///                 111,
///                 "test_key".to_string(),
///                 NodeType::Daemon,
///                 ConnectType::Tcp,
///             );
///             hsession::admin_session(ActionType::Add(hs)).await;
///         });
///
///         let t2 = ylong_runtime::spawn(async {
///             let _ = ylong_runtime::time::sleep(Duration::from_millis(200)).await;
///             let opt = hsession::admin_session(ActionType::Query(111)).await;
///             let lock = opt.unwrap();
///             let hs = lock.lock().await;
///             assert_eq!(hs.connect_key, "test_key".to_string());
///         });
///
///         t1.await.unwrap();
///         t2.await.unwrap();
///     }
///
///     #[ylong_runtime::test]
///     async fn if_hsession_query_ref_work() {
///         let t1 = ylong_runtime::spawn(async {
///             let hs = HdcSession::new(
///                 222,
///                 "test_key".to_string(),
///                 NodeType::Daemon,
///                 ConnectType::Tcp,
///             );
///             hsession::admin_session(ActionType::Add(hs)).await;
///         });
///
///         let t2 = ylong_runtime::spawn(async {
///             let _ = ylong_runtime::time::sleep(Duration::from_millis(200)).await;
///             let _ = hsession::admin_session(ActionType::QueryRef(222)).await;
///         });
///
///         let t3 = ylong_runtime::spawn(async {
///             let _ = ylong_runtime::time::sleep(Duration::from_millis(400)).await;
///             let opt = hsession::admin_session(ActionType::Query(222)).await;
///             let lock = opt.unwrap();
///             let hs = lock.lock().await;
///             assert_eq!(hs.ref_cnt, 1);
///         });
///
///         t1.await.unwrap();
///         t2.await.unwrap();
///         t3.await.unwrap();
///     }
///
///     #[ylong_runtime::test]
///     async fn if_hsession_update_work() {
///         let t1 = ylong_runtime::spawn(async {
///             let hs = HdcSession::new(
///                 333,
///                 "test_key".to_string(),
///                 NodeType::Daemon,
///                 ConnectType::Tcp,
///             );
///             hsession::admin_session(ActionType::Add(hs)).await;
///         });
///
///         let t2 = ylong_runtime::spawn(async {
///             let hs = HdcSession::new(
///                 3333,
///                 "test_key2".to_string(),
///                 NodeType::Server,
///                 ConnectType::Bt,
///             );
///             let _ = ylong_runtime::time::sleep(Duration::from_millis(300)).await;
///             let _ = hsession::admin_session(ActionType::Update(333, hs)).await;
///         });
///
///         let t3 = ylong_runtime::spawn(async {
///             let _ = ylong_runtime::time::sleep(Duration::from_millis(400)).await;
///             let opt = hsession::admin_session(ActionType::Query(3333)).await;
///             let lock = opt.unwrap();
///             let hs = lock.lock().await;
///             assert_eq!(hs.connect_key, "test_key2".to_string());
///             assert!(matches!(hs.connect_type, ConnectType::Bt));
///             assert!(matches!(hs.node_type, NodeType::Server));
///         });
///
///         let t4 = ylong_runtime::spawn(async {
///             let _ = ylong_runtime::time::sleep(Duration::from_millis(500)).await;
///             let opt = hsession::admin_session(ActionType::Query(333)).await;
///             assert!(opt.is_none());
///         });
///
///         t1.await.unwrap();
///         t2.await.unwrap();
///         t3.await.unwrap();
///         t4.await.unwrap();
///     }
///
///     #[ylong_runtime::test]
///     async fn if_hsession_update_outside_admin_work() {
///         let t1 = ylong_runtime::spawn(async {
///             let hs = HdcSession::new(
///                 444,
///                 "test_key".to_string(),
///                 NodeType::Daemon,
///                 ConnectType::Tcp,
///             );
///             hsession::admin_session(ActionType::Add(hs)).await;
///         });
///
///         let t2 = ylong_runtime::spawn(async {
///             let _ = ylong_runtime::time::sleep(Duration::from_millis(200)).await;
///             let opt = hsession::admin_session(ActionType::Query(444)).await;
///             let lock = opt.unwrap();
///             let mut hs = lock.lock().await;
///             hs.connect_key = "new_key".to_string();
///         });
///
///         let t3 = ylong_runtime::spawn(async {
///             let _ = ylong_runtime::time::sleep(Duration::from_millis(400)).await;
///             let opt = hsession::admin_session(ActionType::Query(444)).await;
///             let lock = opt.unwrap();
///             let hs = lock.lock().await;
///             assert_eq!(hs.connect_key, "new_key".to_string());
///         });
///
///         t1.await.unwrap();
///         t2.await.unwrap();
///         t3.await.unwrap();
///     }
/// }
///
/// #[cfg(test)]
/// mod file_test {
///     use crate::{
///         common::{base::Base, hdcfile::HdcFile},
///         serializer::{serialize::Serialization, native_struct::TransferConfig},
///     };
///     #[ylong_runtime::test]
///     async fn test_base_fn() {
///         let command = String::from(
///             "-cwd \"C:\\Users\\\" C:\\Users\\Desktop\\hdcfile\\hdcd_system /data/hdcd",
///         );
///         let mut argc = 0;
///         let argv = Base::split_command_to_args(&command, &mut argc);
///         assert_eq!(argc, 4);
///         assert_eq!(argv.len(), 4);
///         assert_eq!(argv.get(0), Some(&"-cwd".to_string()));
///         assert_eq!(argv.get(1), Some(&"C:\\Users\\".to_string()));
///         assert_eq!(
///             argv.get(2),
///             Some(&"C:\\Users\\Desktop\\hdcfile\\hdcd_system".to_string())
///         );
///         assert_eq!(argv.get(3), Some(&"/data/hdcd".to_string()));
///     }
///
///     #[ylong_runtime::test]
///     async fn test_get_file_name() {
///         let mut path = String::from("/home/test/hdctest.log");
///         let file_name = Base::get_file_name(&mut path).unwrap();
///         assert_eq!(file_name, "hdctest.log");
///     }
///
///     #[ylong_runtime::test]
///     async fn test_file_task_master() {
///         let mut task = HdcFile::new(1, 2);
///         let mut command = String::from("-cwd \"C:\\Users\\\" /system/bin /data/hdcd");
///         if cfg!(target_os = "windows") {
///             command = String::from("-cwd \"C:\\Users\\\" C:\\Users\\ /data/");
///         }
///         let result = task.begin_transfer(&command);
///         if !result {
///             assert!(true);
///         } else {
///             assert_eq!(task.transfer.is_dir, true);
///             println!("{}", task.transfer.base_local_path);
///             if cfg!(target_os = "linux") {
///                 assert_eq!(task.transfer.base_local_path, String::from("/system/bin/"));
///             } else {
///                 assert_eq!(task.transfer.base_local_path, String::from("C:\\Users\\"));
///             }
///             assert_eq!(task.transfer.remote_path, String::from("/data/hdcd"));
///             assert_ne!(task.transfer.task_queue.len(), 0);
///         }
///     }
///
///     #[ylong_runtime::test]
///     async fn test_file_task_slave() {
///         let mut task = HdcFile::new(1, 2);
///         let mut transfer_config = TransferConfig::default();
///         transfer_config.file_size = 88888888;
///         transfer_config.path = "/data/hdcd".to_string();
///         transfer_config.optional_name = "hdcd".to_string();
///         let payload = TransferConfig::serialize(&transfer_config);
///         task.check_slaver(&payload[..]);
///         assert_eq!(task.transfer.file_size, 88888888);
///         assert_eq!(task.transfer.local_path, String::from("/data/hdcd"));
///     }
/// }
#[cfg(test)]
mod sync_session_test {}
