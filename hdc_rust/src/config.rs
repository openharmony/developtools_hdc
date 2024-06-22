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
//! config
#![allow(missing_docs)]

use std::convert::TryFrom;

use log::LevelFilter;

pub enum CompressType {
    None = 0,
    Lz4,
    Lz77,
    Lzma,
    Brotli,
}

impl TryFrom<u8> for CompressType {
    type Error = ();
    fn try_from(cmd: u8) -> Result<Self, ()> {
        match cmd {
            0 => Ok(Self::None),
            1 => Ok(Self::Lz4),
            2 => Ok(Self::Lz77),
            3 => Ok(Self::Lzma),
            4 => Ok(Self::Brotli),
            _ => Err(()),
        }
    }
}

#[allow(unused)]
#[derive(Clone, Default, Debug)]
pub enum ConnectType {
    Usb(String),
    #[default]
    Tcp,
    Uart,
    Bt,
    Bridge,
    HostUsb(String),
}

pub enum ErrCode {
    Success = 0,
    ModuleJdwpFailed = -18000,
}

pub enum NodeType {
    Server,
    Daemon,
}

#[derive(Debug, Clone)]
pub struct TaskMessage {
    pub channel_id: u32,
    pub command: HdcCommand,
    pub payload: Vec<u8>,
}

#[derive(PartialEq, Debug, Clone, Copy)]
#[repr(u32)]
pub enum HdcCommand {
    KernelHelp = 0,
    KernelHandshake,
    KernelChannelClose,
    KernelServerKill,
    KernelTargetDiscover,
    KernelTargetList,
    KernelTargetAny,
    KernelTargetConnect,
    KernelTargetDisconnect,
    KernelEcho,
    KernelEchoRaw,
    KernelEnableKeepalive,
    KernelWakeupSlavetask,
    KernelCheckServer,
    KernelCheckDevice,
    KernelWaitFor,

    // New in refactor
    KernelServerStart,
    ClientVersion,
    ClientKeyGenerate,

    // One-pass simple commands
    UnityCommandHead = 1000, // not use
    UnityExecute,
    UnityRemount,
    UnityReboot,
    UnityRunmode,
    UnityHilog,
    UnityTerminate,
    UnityRootrun,
    JdwpList,
    JdwpTrack,
    UnityCommandTail, // not use
    // It will be separated from unity in the near future
    UnityBugreportInit,
    UnityBugreportData,
    // Shell commands types
    ShellInit = 2000,
    ShellData,
    // Forward commands types
    ForwardInit = 2500,
    ForwardCheck,
    ForwardCheckResult,
    ForwardActiveSlave,
    ForwardActiveMaster,
    ForwardData,
    ForwardFreeContext,
    ForwardList,
    ForwardRemove,
    ForwardSuccess,
    ForwardRportInit,
    ForwardRportList,
    ForwardRportRemove,
    // File commands
    FileInit = 3000,
    FileCheck,
    FileBegin,
    FileData,
    FileFinish,
    AppSideload,
    FileMode,
    DirMode,
    FileRecvInit,
    // App commands
    AppInit = 3500,
    AppCheck,
    AppBegin,
    AppData,
    AppFinish,
    AppUninstall,
    // Flashd commands
    FlashdUpdateInit = 4000,
    FlashdFlashInit,
    FlashdCheck,
    FlashdBegin,
    FlashdData,
    FlashdFinish,
    FlashdErase,
    FlashdFormat,
    FlashdProgress,

    UartFinish,
}

impl TryFrom<u32> for HdcCommand {
    type Error = ();
    fn try_from(cmd: u32) -> Result<Self, ()> {
        match cmd {
            0 => Ok(Self::KernelHelp),
            1 => Ok(Self::KernelHandshake),
            2 => Ok(Self::KernelChannelClose),
            3 => Ok(Self::KernelServerKill),
            4 => Ok(Self::KernelTargetDiscover),
            5 => Ok(Self::KernelTargetList),
            6 => Ok(Self::KernelTargetAny),
            7 => Ok(Self::KernelTargetConnect),
            8 => Ok(Self::KernelTargetDisconnect),
            9 => Ok(Self::KernelEcho),
            10 => Ok(Self::KernelEchoRaw),
            11 => Ok(Self::KernelEnableKeepalive),
            12 => Ok(Self::KernelWakeupSlavetask),
            13 => Ok(Self::KernelCheckServer),
            14 => Ok(Self::KernelCheckDevice),
            15 => Ok(Self::KernelWaitFor),

            1000 => Ok(Self::UnityCommandHead),
            1001 => Ok(Self::UnityExecute),
            1002 => Ok(Self::UnityRemount),
            1003 => Ok(Self::UnityReboot),
            1004 => Ok(Self::UnityRunmode),
            1005 => Ok(Self::UnityHilog),
            1006 => Ok(Self::UnityTerminate),
            1007 => Ok(Self::UnityRootrun),
            1008 => Ok(Self::JdwpList),
            1009 => Ok(Self::JdwpTrack),
            1010 => Ok(Self::UnityCommandTail),
            1011 => Ok(Self::UnityBugreportInit),
            1012 => Ok(Self::UnityBugreportData),

            2000 => Ok(Self::ShellInit),
            2001 => Ok(Self::ShellData),

            2500 => Ok(Self::ForwardInit),
            2501 => Ok(Self::ForwardCheck),
            2502 => Ok(Self::ForwardCheckResult),
            2503 => Ok(Self::ForwardActiveSlave),
            2504 => Ok(Self::ForwardActiveMaster),
            2505 => Ok(Self::ForwardData),
            2506 => Ok(Self::ForwardFreeContext),
            2507 => Ok(Self::ForwardList),
            2508 => Ok(Self::ForwardRemove),
            2509 => Ok(Self::ForwardSuccess),

            3000 => Ok(Self::FileInit),
            3001 => Ok(Self::FileCheck),
            3002 => Ok(Self::FileBegin),
            3003 => Ok(Self::FileData),
            3004 => Ok(Self::FileFinish),
            3005 => Ok(Self::AppSideload),
            3006 => Ok(Self::FileMode),
            3007 => Ok(Self::DirMode),

            3500 => Ok(Self::AppInit),
            3501 => Ok(Self::AppCheck),
            3502 => Ok(Self::AppBegin),
            3503 => Ok(Self::AppData),
            3504 => Ok(Self::AppFinish),
            3505 => Ok(Self::AppUninstall),

            4000 => Ok(Self::FlashdUpdateInit),
            4001 => Ok(Self::FlashdFlashInit),
            4002 => Ok(Self::FlashdCheck),
            4003 => Ok(Self::FlashdBegin),
            4004 => Ok(Self::FlashdData),
            4005 => Ok(Self::FlashdFinish),
            4006 => Ok(Self::FlashdErase),
            4007 => Ok(Self::FlashdFormat),
            4008 => Ok(Self::FlashdProgress),

            _ => Err(()),
        }
    }
}

#[allow(unused)]
pub enum AuthType {
    None,
    Token,
    Signature,
    Publickey,
    OK,
    Fail,
}

pub enum AppModeType {
    Install = 1,
    UnInstall,
}

impl TryFrom<u8> for AppModeType {
    type Error = ();
    fn try_from(cmd: u8) -> Result<Self, ()> {
        match cmd {
            1 => Ok(Self::Install),
            2 => Ok(Self::UnInstall),
            _ => Err(()),
        }
    }
}

#[repr(u8)]
pub enum MessageLevel {
    Fail,
    Info,
    Ok,
}

#[allow(unused)]
#[derive(Clone, Default, Debug)]
pub enum ContextType {
    Any,
    #[default]
    App,
    File,
    Shell,
    Forward,
    ExecuteShell,
}

pub const PACKET_FLAG: &[u8] = "HW".as_bytes();
pub const VER_PROTOCOL: u16 = 1;
pub const ENABLE_IO_CHECK: bool = false;
pub const PAYLOAD_VCODE: u8 = 0x09;
pub const HDC_BUF_MAX_SIZE: usize = 0x7fffffff;
pub const HANDSHAKE_MESSAGE: &str = "OHOS HDC";
pub const BANNER_SIZE: usize = 12;
pub const KEY_MAX_SIZE: usize = 32;
pub const FILE_PACKAGE_HEAD: usize = 64;
pub const FILE_PACKAGE_PAYLOAD_SIZE: usize = 49152;
pub const KERNEL_FILE_NODE_SIZE: u16 = 1024 * 4; // kernel file node 4K buffer
pub const MAX_PACKET_SIZE_HISPEED: i32 = 512;
pub const MAX_SIZE_IOBUF: usize = 61440;

pub const WIN_CMD_PROG: &str = "cmd.exe";
pub const SHELL_PROG: &str = "sh";
pub const SHELL_TEMP: &str = "/data/local/tmp/hdc-pty";

pub const LOG_FILE_NAME: &str = "hdc";
pub const LOG_BAK_NAME: &str = "hdclast";
pub const LOG_TAIL_NAME: &str = ".log";
pub const LOG_CACHE_NAME: &str = ".hdc.cache";
pub const LOG_FILE_SIZE: usize = 1024 * 1024 * 100; // 100MB

pub const DAEMON_PORT: u16 = 60000;
pub const SERVER_DEFAULT_PORT: u16 = 9710;
pub const MAX_PORT_NUM: u32 = 65535;
pub const MAX_PORT_LEN: usize = 5;
pub const IPV4_MAPPING_PREFIX: &str = "::ffff:";
pub const LOCAL_HOST: &str = "127.0.0.1";

pub const UART_NODE: &str = "/dev/ttyS4";
pub const UART_DEFAULT_BAUD_RATE: i32 = 1500000;
pub const UART_DEFAULT_BITS: i32 = 8;
pub const UART_EVENT: u8 = 78;
pub const MAX_UART_SIZE_IOBUF: u32 = 4096;

pub const USB_FFS_BASE: &str = "/dev/usb-ffs/";
pub const USB_PACKET_FLAG: &[u8] = "UB".as_bytes();
pub const USB_QUEUE_LEN: usize = 64;

pub const TRANSFER_FUNC_NAME: &str = "install";
pub const INSTALL_TMP_DIR: &str = "/data/local/tmp/";
pub const INSTALL_TAR_MAX_CNT: usize = 512;

pub const ENV_HDC_MODE: &str = "persist.hdc.mode";
pub const ENV_HOST_PORT: &str = "persist.hdc.port";
pub const MODE_USB: &str = "usb";
pub const MODE_TCP: &str = "tcp";
pub const PREFIX_PORT: &str = "port ";
pub const ENV_ROOT_RUN_MODE: &str = "persist.hdc.root";
pub const ENV_STARTUP: &str = "ohos.startup.powerctrl";
pub const ENV_DEBUGGABLE: &str = "const.debuggable";
pub const ENV_SHELL_CONTROL: &str = "persist.hdc.control.shell";
pub const ENV_FILE_CONTROL: &str = "persist.hdc.control.file";
pub const ENV_FPORT_CONTROL: &str = "persist.hdc.control.fport";

pub const RSA_BIT_NUM: usize = 3072;
pub const RSA_PUBKEY_PATH: &str = "/data/service/el0/hdc";
pub const RSA_PUBKEY_NAME: &str = "hdc_keys";
pub const RSA_PRIKEY_PATH: &str = ".harmony";
pub const RSA_PRIKEY_NAME: &str = "hdckey";
// "\f" asicc is 0x0C
pub const HDC_HOST_DAEMON_BUF_SEPARATOR: char = '\x0C';
// the API WaitParameter can only accept max 96 bytes value
pub const HDC_PARAMETER_VALUE_MAX_LEN: usize = 96;
pub const HDC_HOSTNAME_MAX_LEN: usize = HDC_PARAMETER_VALUE_MAX_LEN;
pub const HDC_WAIT_PARAMETER_FOREVER: i32 = 0;
pub const HDC_HANDSHAKE_TOKEN_LEN: usize = 32;

pub const DAEOMN_AUTH_SUCCESS: &str = "SUCCESS";
pub const DAEOMN_UNAUTHORIZED: &str = "DAEMON_UNAUTH";
pub const TLV_TAG_LEN: usize = 16;
pub const TLV_VAL_LEN: usize = 16;
pub const TLV_VAL_MAXLEN: usize = 1024;
pub const TLV_VAL_INVALID_LEN: usize = TLV_VAL_MAXLEN + 1;
pub const TLV_MIN_LEN: usize = TLV_TAG_LEN + TLV_VAL_LEN;
pub const TAG_DEVNAME: &str = "devname";
pub const TAG_HOSTNAME: &str = "hostname";
pub const TAG_PUBKEY: &str = "pubkey";
pub const TAG_EMGMSG: &str = "emgmsg";
pub const TAG_TOKEN: &str = "token";
pub const TAG_DAEOMN_AUTHSTATUS: &str = "daemonauthstatus";

pub const LOG_LEVEL_ORDER: [LevelFilter; 7] = [
    LevelFilter::Off,
    LevelFilter::Error,
    LevelFilter::Warn,
    LevelFilter::Info,
    LevelFilter::Debug,
    LevelFilter::Trace,
    LevelFilter::Trace,
];

// |----------------------------------------------------------------|
// | 31-28 | 27-24 | 23-20 | 19-16 | 15-12 | 11-08 |     07-00      |
// |----------------------------------------------------------------|
// | major |reserve| minor |reserve|version|  fix  |   reserve      |
// |----------------------------------------------------------------|
// 0x30000200 is 3.0.0d
const HDC_VERSION_NUMBER: u32 = 0x30000300;
pub const AUTH_BASE_VERSDION: &str = "Ver: 3.0.0b";
pub fn get_version() -> String {
    let major = (HDC_VERSION_NUMBER >> 28) & 0xff;
    let minor = (HDC_VERSION_NUMBER >> 20) & 0xff;
    let version = (HDC_VERSION_NUMBER >> 12) & 0xff;
    let fix = std::char::from_u32(((HDC_VERSION_NUMBER >> 8) & 0xff) + 0x61).unwrap();
    format!("Ver: {major}.{minor}.{version}{}", fix)
}
