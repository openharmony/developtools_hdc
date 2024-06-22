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
//! utils
#![allow(missing_docs)]

use crate::config::{SHELL_PROG, WIN_CMD_PROG};

use std::io::{self, Error, ErrorKind};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

#[cfg(feature = "host")]
extern crate ylong_runtime_static as ylong_runtime;
use ylong_runtime::io::AsyncWriteExt;

pub fn get_pseudo_random_u32() -> u32 {
    (SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_nanos()
        & 0xffffffff) as u32
}

pub fn get_current_time() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_millis() as u64
}

pub async fn print_msg(buf: Vec<u8>) -> io::Result<()> {
    let mut stdout = ylong_runtime::io::stdout();
    let _ = stdout.write(&buf).await;
    let _ = stdout.flush().await;
    Ok(())
}

pub fn execute_cmd(cmd: String) -> Result<std::process::Output, Error> {
    let arg_sign = if cfg!(target_os = "windows") {
        "/c"
    } else {
        "-c"
    };

    let programe = if cfg!(target_os = "windows") {
        WIN_CMD_PROG
    } else {
        SHELL_PROG
    };

    Command::new(programe).args([arg_sign, &cmd]).output()
}

pub fn error_other(msg: String) -> Error {
    Error::new(ErrorKind::Other, msg)
}

pub fn bytes_to_string(message: Vec<u8>) -> String {
    let msg = String::from_utf8(message);
    match msg {
        Ok(str) => str,
        Err(_e) => "".to_string(),
    }
}

pub mod hdc_log {
    #[cfg(not(feature = "host"))]
    pub use hilog_rust::{hilog, HiLogLabel, LogType};
    #[cfg(not(feature = "host"))]
    pub use std::ffi::{c_char, CString};

    #[cfg(not(feature = "host"))]
    pub const LOG_LABEL: HiLogLabel = HiLogLabel {
        log_type: LogType::LogCore,
        domain: 0xD002D13,
        tag: "HDC_LOG",
    };

    #[cfg(not(feature = "host"))]
    #[inline(never)]
    pub fn get_file_name(path : &str) -> &str {
        path.split('/').last().unwrap()
    }

    #[cfg(not(feature = "host"))]
    #[macro_export]
    macro_rules! trace {
        ($($arg:tt)+) => {
            hilog_rust::debug!(LOG_LABEL, "{}:{} {}", @public(get_file_name(file!())), @public(line!()),  @public(format!($($arg)+)));
            log::trace!($($arg)+);
        };
    }

    #[cfg(feature = "host")]
    #[macro_export]
    macro_rules! trace {
        ($($arg:tt)+) => {
            log::trace!($($arg)+);
        };
    }

    #[cfg(not(feature = "host"))]
    #[macro_export]
    macro_rules! debug {
        ($($arg:tt)+) => {
            hilog_rust::debug!(LOG_LABEL, "{}:{} {}", @public(get_file_name(file!())), @public(line!()),  @public(format!($($arg)+)));
            log::debug!($($arg)+);
        };
    }

    #[cfg(feature = "host")]
    #[macro_export]
    macro_rules! debug {
        ($($arg:tt)+) => {
            log::debug!($($arg)+);
        };
    }

    #[cfg(not(feature = "host"))]
    #[macro_export]
    macro_rules! info {
        ($($arg:tt)+) => {
            hilog_rust::info!(LOG_LABEL, "{}:{} {}", @public(get_file_name(file!())), @public(line!()),  @public(format!($($arg)+)));
            log::info!($($arg)+);
        };
    }

    #[cfg(feature = "host")]
    #[macro_export]
    macro_rules! info {
        ($($arg:tt)+) => {
            log::info!($($arg)+);
        };
    }

    #[cfg(not(feature = "host"))]
    #[macro_export]
    macro_rules! warn {
        ($($arg:tt)+) => {
            hilog_rust::warn!(LOG_LABEL, "{}:{} {}", @public(get_file_name(file!())), @public(line!()),  @public(format!($($arg)+)));
            log::warn!($($arg)+);
        };
    }

    #[cfg(feature = "host")]
    #[macro_export]
    macro_rules! warn {
        ($($arg:tt)+) => {
            log::warn!($($arg)+);
        };
    }

    #[cfg(not(feature = "host"))]
    #[macro_export]
    macro_rules! error {
        ($($arg:tt)+) => {
            hilog_rust::error!(LOG_LABEL, "{}:{} {}", @public(get_file_name(file!())), @public(line!()),  @public(format!($($arg)+)));
            log::error!($($arg)+);
        };
    }

    #[cfg(feature = "host")]
    #[macro_export]
    macro_rules! error {
        ($($arg:tt)+) => {
            log::error!($($arg)+);
        };
    }

    #[cfg(not(feature = "host"))]
    #[macro_export]
    macro_rules! fatal {
        ($($arg:tt)+) => {
            hilog_rust::fatal!(LOG_LABEL, "{}:{} {}", @public(get_file_name(file!())), @public(line!()),  @public(format!($($arg)+)));
            log::fatal!($($arg)+);
        };
    }

    #[cfg(feature = "host")]
    #[macro_export]
    macro_rules! fatal {
        ($($arg:tt)+) => {
            log::fatal!($($arg)+);
        };
    }

    // pub use crate::{trace, debug, info, warn, error, fatal};
}