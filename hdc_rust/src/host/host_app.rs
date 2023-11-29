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
use hdc::common::base::Base;
use hdc::common::filemanager::FileManager;
use hdc::common::hdctransfer::{self, HdcTransferBase};
use hdc::common::taskbase::TaskBase;
use hdc::config;
use hdc::config::HdcCommand;
use hdc::config::TaskMessage;
use hdc::config::TRANSFER_FUNC_NAME;
use hdc::serializer::serialize::Serialization;
use hdc::transfer;
use hdc::transfer::EchoLevel;
use hdc::utils;
use std::path::PathBuf;

pub struct HostAppTask {
    pub transfer: HdcTransferBase,
}

impl HostAppTask {
    pub fn new(_session_id: u32, _channel_id: u32) -> Self {
        Self {
            transfer: HdcTransferBase::new(_session_id, _channel_id),
        }
    }

    pub fn get_sub_app_files_resurively(_path: &String) -> Vec<String> {
        let mut result = Vec::new();
        let dir_path = PathBuf::from(_path);
        for entry in std::fs::read_dir(dir_path).unwrap() {
            let path = entry.unwrap().path();
            let p = path.display().to_string();
            if p.ends_with(".hap") || p.ends_with(".hsp") {
                result.push(p.clone());
            } else {
                let mut sub_list = Self::get_sub_app_files_resurively(&p);
                result.append(&mut sub_list);
            }
        }
        result.sort();
        result
    }

    pub fn init_install(&mut self, command: &String) -> bool {
        let (argv, argc) = Base::split_command_to_args(command);
        if argc < 1 {
            return false;
        }
        let mut i = 0usize;
        let mut options = String::from("");
        while i < argc as usize {
            if argv[i] == "-cwd" {
                if i + 1 < argc as usize {
                    self.transfer.transfer_config.client_cwd = argv[i + 1].clone();
                    i += 1;
                }
            } else if argv[i].starts_with("-") {
                if !options.is_empty() {
                    options.push(' ');
                }
                options.push_str(&mut argv[i].clone());
            } else {
                let mut path = argv[i].clone() as String;
                path = Base::extract_relative_path(
                    &self.transfer.transfer_config.client_cwd,
                    path.as_str(),
                );
                if path.ends_with(".hap") || path.ends_with(".hsp") {
                    self.transfer.task_queue.push(path.clone());
                } else {
                    let mut queue = Self::get_sub_app_files_resurively(&path);
                    self.transfer.task_queue.append(&mut queue);
                }
            }
            i += 1;
        }

        if self.transfer.task_queue.is_empty() {
            return false;
        }

        self.transfer.transfer_config.options = options.clone();
        self.transfer.transfer_config.function_name = TRANSFER_FUNC_NAME.to_string();

        self.transfer.is_master = true;
        self.install_single();

        true
    }

    fn install_single(&mut self) {
        self.transfer.local_path = self.transfer.task_queue.pop().unwrap();
        let local_path = self.transfer.local_path.clone();
        let mut file_manager = FileManager::new(local_path.clone());
        let open_result = file_manager.open();
        if open_result {
            let config = &mut self.transfer.transfer_config;
            config.file_size = file_manager.file_size();
            self.transfer.file_size = config.file_size;
            config.optional_name = utils::get_pseudo_random_u32().to_string();
            if let Some(index) = local_path.rfind('.') {
                let str = local_path.as_str();
                config.optional_name.push_str(&str[index..]);
            }
            if config.hold_timestamp {}
            config.path = self.transfer.remote_path.clone();
        } else {
            self.task_finish();
        }
    }

    fn put_app_check(&mut self) {
        let file_check_message = TaskMessage {
            channel_id: self.transfer.channel_id,
            command: HdcCommand::AppCheck,
            payload: self.transfer.transfer_config.serialize(),
        };
        let send_msg_task = async {
            transfer::put(self.transfer.session_id, file_check_message).await;
        };
        ylong_runtime::block_on(send_msg_task);
    }

    fn do_app_finish(&mut self, _payload: &Vec<u8>) -> bool {
        let mode = config::AppModeType::try_from(_payload[0]);
        if let Ok(mode_type) = mode {
            let str = String::from_utf8(_payload[2..].to_vec()).unwrap();
            return self.check_install_continue(mode_type, str.clone());
        }
        false
    }

    fn do_app_uninstall(&mut self, _payload: &Vec<u8>) {
        let app_uninstall_message = TaskMessage {
            channel_id: self.transfer.channel_id,
            command: HdcCommand::AppUninstall,
            payload: _payload.to_vec(),
        };
        let send_msg_task = async {
            transfer::put(self.transfer.session_id, app_uninstall_message).await;
        };
        ylong_runtime::block_on(send_msg_task);
    }

    fn check_install_continue(&mut self, mode_type: config::AppModeType, str: String) -> bool {
        let mut _mode_desc = String::from("");
        match mode_type {
            config::AppModeType::Install => _mode_desc = String::from("App install"),
            config::AppModeType::UnInstall => _mode_desc = String::from("App uninstall"),
        }
        let message = format!(
            "{}, path:{}, queuesize:{}, msg:{}",
            _mode_desc.clone(),
            self.transfer.local_path.clone(),
            self.transfer.task_queue.len(),
            str.clone()
        );
        self.echo_client(message);
        if self.transfer.task_queue.is_empty() {
            self.echo_client(String::from("AppMod finish"));
            self.task_finish();
            hdctransfer::close_channel(self.channel_id());
            return false;
        }
        self.install_single();
        true
    }

    fn echo_client(&mut self, message: String) {
        ylong_runtime::block_on(async {
            let _ = transfer::send_channel_msg(self.channel_id(), EchoLevel::INFO, message).await;
        });
    }
}

impl TaskBase for HostAppTask {
    fn command_dispatch(
        &mut self,
        _command: HdcCommand,
        _payload: &[u8],
        _payload_size: u16,
    ) -> bool {
        match _command {
            HdcCommand::AppInit => {
                let s = String::from_utf8(_payload.to_vec());
                match s {
                    Ok(str) => {
                        if self.init_install(&str) {
                            self.put_app_check();
                        }
                    }
                    Err(e) => {
                        println!("error {}", e);
                    }
                }
            }
            HdcCommand::AppBegin => {
                hdctransfer::transfer_begin(&self.transfer, HdcCommand::AppData);
            }
            HdcCommand::AppUninstall => {
                self.do_app_uninstall(&_payload.to_vec());
            }
            HdcCommand::AppFinish => {
                self.do_app_finish(&_payload.to_vec());
            }
            _ => {
                println!("other command");
            }
        }
        true
    }

    fn stop_task(&mut self) {}

    fn ready_for_release(&mut self) -> bool {
        true
    }

    fn channel_id(&self) -> u32 {
        self.transfer.channel_id
    }

    fn task_finish(&self) {
        hdctransfer::transfer_task_finish(self.transfer.channel_id, self.transfer.session_id);
    }
}
