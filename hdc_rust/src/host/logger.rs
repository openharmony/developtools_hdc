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
use std::io::Write;
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::time::SystemTime;

use hdc::config;

#[derive(Default)]

pub struct LoggerMeta {
    stdout_require: bool,
    run_in_server: bool, // scrolling dump only by server
    current_size: usize,
    log_file: std::path::PathBuf,
}

type LoggerMeta_ = Arc<Mutex<LoggerMeta>>;

struct HostLoggerMeta {}
impl HostLoggerMeta {
    fn get_instance() -> LoggerMeta_ {
        static mut LOGGER_META: Option<LoggerMeta_> = None;
        unsafe {
            LOGGER_META
                .get_or_insert_with(|| Arc::new(Mutex::new(LoggerMeta::default())))
                .clone()
        }
    }

    fn init(run_in_server: bool, spawned_server: bool) {
        let instance = Self::get_instance();
        let mut meta = instance.lock().unwrap();
        if run_in_server && !spawned_server {
            meta.stdout_require = true;
        }
        meta.run_in_server = run_in_server;
        meta.log_file = Path::new(&std::env::temp_dir()).join(config::LOG_FILE_NAME);
        if run_in_server {
            std::fs::File::create(&meta.log_file).unwrap();
        }
    }

    fn write_log(content: String) {
        let instance = Self::get_instance();
        let mut meta = instance.lock().unwrap();
        if meta.run_in_server && meta.current_size > config::LOG_FILE_SIZE {
            meta.current_size = 0;
            // TODO: mv to new file
        }
        meta.current_size += content.len();
        if let Ok(mut f) = std::fs::File::options().append(true).open(&meta.log_file) {
            writeln!(&mut f, "{}", content).unwrap();
        }
        if meta.stdout_require {
            println!("{}", content);
        }
    }
}

struct SimpleHostLogger;
impl log::Log for SimpleHostLogger {
    fn enabled(&self, metadata: &log::Metadata) -> bool {
        metadata.level() <= log::max_level()
    }
    fn log(&self, record: &log::Record) {
        if self.enabled(record.metadata()) {
            let ts = humantime::format_rfc3339_millis(SystemTime::now()).to_string();
            let level = &record.level().to_string()[..1];
            let file = record.file().unwrap();
            // cargo编译下的文件目录可能存在\\的目录，需要通过编译宏隔离
            #[cfg(target_os = "windows")]
            let file = file.replace('\\', "/");
            let content = format!(
                "{} {} {} {}:{} - {}",
                &ts[..10],
                &ts[11..23],
                level,
                file.split_once('/').unwrap().1,
                record.line().unwrap(),
                record.args()
            );
            HostLoggerMeta::write_log(content);
        }
    }
    fn flush(&self) {}
}

static LOGGER: SimpleHostLogger = SimpleHostLogger;

pub fn logger_init(log_level: log::LevelFilter, run_in_server: bool, spawned_server: bool) {
    HostLoggerMeta::init(run_in_server, spawned_server);
    log::set_logger(&LOGGER).unwrap();
    log::set_max_level(log_level);
}
