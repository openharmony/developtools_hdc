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
 //! host server & client

mod auth;
mod client;
mod host_app;
mod logger;
mod parser;
mod server;
mod task;
mod translate;
mod unittest;

use std::io::ErrorKind;

use hdc::config;

#[macro_use]
extern crate lazy_static;

// static LOGGER: SimpleHostLogger = SimpleHostLogger;

// fn logger_init(log_level: log::LevelFilter) {
//     let log_file: std::path::PathBuf = Path::new(&std::env::temp_dir()).join(config::LOG_FILE_NAME);
//     let _ = std::fs::File::create(log_file);
//     let logger: &'static SimpleHostLogger = &SimpleHostLogger { background_mode: false, flushed_size: 100 };
//     log::set_logger(logger).unwrap();
//     log::set_max_level(log_level);
// }

fn main() {
    let _ = ylong_runtime::builder::RuntimeBuilder::new_multi_thread()
        .worker_stack_size(16 * 1024 * 1024)
        .worker_num(64)
        .keep_alive_time(std::time::Duration::from_secs(10))
        .build_global();

    let parsed_cmd = match parser::parse_command(std::env::args()) {
        Ok(parsed_cmd) => parsed_cmd,
        Err(e) => {
            println!("{}", e);
            return;
        }
    };

    logger::logger_init(
        config::LOG_LEVEL_ORDER[parsed_cmd.log_level],
        parsed_cmd.run_in_server,
        parsed_cmd.spawned_server,
    );

    hdc::debug!("parsed cmd: {:#?}", parsed_cmd);

    if parsed_cmd.run_in_server {
        ylong_runtime::block_on(async {
            let _ = server::run_server_mode(parsed_cmd.server_addr).await;
        });
    } else {
        hdc::debug!(
            "in client mode, cmd: {:#?}, parameter:{:#?}",
            parsed_cmd.command.unwrap(),
            parsed_cmd.parameters
        );
        ylong_runtime::block_on(async {
            if parsed_cmd.command.is_none() {
                println!("Unknown operation command...");
                println!("{}", translate::usage());
                return;
            }

            if let Err(e) = client::run_client_mode(parsed_cmd).await {
                match e.kind() {
                    ErrorKind::Other => println!("[Fail]{}", e),
                    _ => {hdc::trace!("client exit with err: {e:?}");},
                }
            }
        })
    }
}
