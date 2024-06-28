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
//! daemon

extern crate panic_handler;
use hdc::common::jdwp::Jdwp;
use hdc::common::base::Base;
use hdc::config;
use hdc::daemon_lib::*;
use hdc::utils::hdc_log::*;
use hdc::utils;
use std::io::Write;
use std::time::SystemTime;
#[cfg(feature = "emulator")]
use hdc::daemon_lib::bridge;
use hdc::daemon_lib::auth::clear_auth_pub_key_file;
use log::LevelFilter;

fn logger_init(log_level: LevelFilter) {
    env_logger::Builder::new()
        .format(|buf, record| {
            let ts = humantime::format_rfc3339_millis(SystemTime::now()).to_string();
            let level = &record.level().to_string()[..1];
            let file = record.file().unwrap_or("unknown");
            writeln!(
                buf,
                "{} {} {} {}:{} - {}",
                &ts[..10],
                &ts[11..23],
                level,
                file.split('/').last().unwrap_or("unknown"),
                record.line().unwrap_or(0),
                record.args()
            )
        })
        .filter(None, log_level)
        .init();
}

fn get_logger_lv() -> LevelFilter {
    let lv = std::env::var_os("HDCD_LOGLV")
        .unwrap_or_default()
        .to_str()
        .unwrap_or_default()
        .parse::<usize>()
        .unwrap_or(0_usize);
    config::LOG_LEVEL_ORDER[lv]
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    panic_handler::init();
    if args.len() == 2 && args[1] == "-v" {
        println!("{}", config::get_version());
        return;
    }
    logger_init(get_logger_lv());

    let _ = ylong_runtime::builder::RuntimeBuilder::new_multi_thread()
        .worker_stack_size(16 * 1024 * 1024)
        .worker_num(20)
        .max_blocking_pool_size(64)
        .build_global();

    #[cfg(not(feature = "emulator"))]
    need_drop_root_privileges();
    Base::init_process();
    clear_auth_pub_key_file();

    ylong_runtime::block_on(async {
        #[cfg(not(feature = "emulator"))]
        let tcp_task = utils::spawn(async {
            if let Err(e) = tcp_daemon_start(get_tcp_port()).await {
                hdc::error!("[Fail]tcp daemon failed: {}", e);
            }
        });
        #[cfg(not(feature = "emulator"))]
        let usb_task = utils::spawn(async {
            if let Err(e) = usb_daemon_start().await {
                hdc::error!("[Fail]usb daemon failed: {}", e);
            }
        });
        #[cfg(not(feature = "emulator"))]
        let uart_task = utils::spawn(async {
            if let Err(e) = uart_daemon_start().await {
                hdc::error!("[Fail]uart daemon failed: {}", e);
            }
        });
        #[cfg(feature = "emulator")]
        hdc::info!("daemon main emulator, start bridge daemon.");
        #[cfg(feature = "emulator")]
        let bridge_task = utils::spawn(async {
            if let Err(e) = bridge_daemon_start().await {
                hdc::error!("[Fail]bridge daemon failed: {}", e);
            }
        });
        let lock_value = Jdwp::get_instance();
        let jdwp_server_task = utils::spawn(async {
            jdwp_daemon_start(lock_value).await;
        });
        #[cfg(not(feature = "emulator"))]
        let _ = tcp_task.await;
        #[cfg(not(feature = "emulator"))]
        let _ = usb_task.await;
        #[cfg(not(feature = "emulator"))]
        let _ = uart_task.await;
        #[cfg(feature = "emulator")]
        let _ = bridge_task.await;
        let _ = jdwp_server_task.await;
    });
}
