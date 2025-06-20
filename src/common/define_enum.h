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
#ifndef DEFINE_ENUM_H
#define DEFINE_ENUM_H

namespace Hdc {
enum MessageLevel {
    MSG_FAIL,
    MSG_INFO,
    MSG_OK,
};

enum ConnType { CONN_USB = 0, CONN_TCP, CONN_SERIAL, CONN_BT, CONN_UNKNOWN };
const std::string conTypeDetail[] = { "USB", "TCP", "UART", "BT", "UNKNOWN" };

#ifdef HDC_SUPPORT_UART
enum UartTimeConst {
    UV_TIMEOUT = 10,
    UV_REPEAT = 100,
    TIMEOUTS_R_INTERALTIMEOUT = 1000,
    TIMEOUTS_R_TOALTIMEOUTMULTIPLIER = 500,
    TIMEOUTS_R_TIMEOUTCONSTANT = 5000
};
enum UartSetSerialNBits {
    UART_BIT1 = 7,
    UART_BIT2 = 8
};
enum UartSetSerialNSpeed {
    UART_SPEED2400 = 2400,
    UART_SPEED4800 = 4800,
    UART_SPEED9600 = 9600,
    UART_SPEED115200 = 115200,
    UART_SPEED921600 = 921600,
    UART_SPEED1500000 = 1500000
};
enum UartSetSerialNStop {
    UART_STOP1 = 1,
    UART_STOP2 = 2
};
#endif
enum ConnStatus { STATUS_UNKNOW = 0, STATUS_READY, STATUS_CONNECTED, STATUS_OFFLINE, STATUS_UNAUTH };
const std::string conStatusDetail[] = { "Unknown", "Ready", "Connected", "Offline", "Unauthorized" };

enum AuthVerifyType { RSA_ENCRYPT = 0, RSA_3072_SHA512 = 1, PSK_TLS_AES_128_GCM_SHA256 = 2, UNKNOWN = 100 };

enum OperateID {
    OP_ADD,
    OP_REMOVE,
    OP_QUERY,
    OP_QUERY_REF,  // crossthread query, manually reduce ref
    OP_GET_STRLIST,
    OP_GET_STRLIST_FULL,
    OP_GET_ANY,
    OP_UPDATE,
    OP_CLEAR,
    OP_INIT,
    OP_GET_ONLY,
    OP_VOTE_RESET,
    OP_WAIT_FOR_ANY,
    OP_PRINT
};

enum RetErrCode {
    RET_SSL_HANDSHAKE_FINISHED = 1,
    RET_SUCCESS = 0,
    ERR_GENERIC = -1,
    ERR_NO_SUPPORT = -2,
    ERR_PARAM_NULLPTR = -5,
    ERR_LIBUSB_OPEN = -100,
    ERR_BUF_SIZE = -10000,
    ERR_BUF_ALLOC,
    ERR_BUF_OVERFLOW,
    ERR_BUF_CHECK,
    ERR_BUF_RESET,
    ERR_BUF_COPY,
    ERR_CHECK_VERSION,
    ERR_FILE_OPEN = -11000,
    ERR_FILE_READ,
    ERR_FILE_WRITE,
    ERR_FILE_STAT,
    ERR_FILE_PATH_CHECK,
    ERR_PARM_FORMAT = -12000,
    ERR_PARM_SIZE,
    ERR_PARM_FAIL,
    ERR_API_FAIL = -13000,
    ERR_IO_FAIL = -14000,
    ERR_IO_TIMEOUT,
    ERR_IO_SOFT_RESET,
    ERR_SESSION_NOFOUND = -15000,
    ERR_SESSION_OFFLINE,
    ERR_SESSION_DEAD,
    ERR_HANDSHAKE_NOTMATCH = -16000,
    ERR_HANDSHAKE_CONNECTKEY_FAILED,
    ERR_HANDSHAKE_HANGUP_CHILD,
    ERR_SOCKET_FAIL = -17000,
    ERR_SOCKET_DUPLICATE,
    ERR_MODULE_JDWP_FAILED = -18000,
    ERR_UT_MODULE_NOTREADY = -19000,
    ERR_UT_MODULE_WAITMAX,
    ERR_THREAD_MUTEX_FAIL = -20000,
    ERR_PROCESS_SUB_FAIL = -21000,
    ERR_PRIVELEGE_NEED = -22000,
    ERR_ENCRYPT = -23000,
    ERR_DECRYPT = -23001,
    ERR_DECRYPT_WANT_READ = -23002,
};

// Flags shared by multiple modules
enum AsyncEvent {
    ASYNC_STOP_MAINLOOP = 0,
    ASYNC_FREE_SESSION,
    ASYNC_FREE_CHANNEL,
};
enum InnerCtrlCommand {
    SP_START_SESSION = 0,
    SP_STOP_SESSION,
    SP_ATTACH_CHANNEL,
    SP_DEATCH_CHANNEL,
    SP_JDWP_NEWFD,
    SP_ARK_NEWFD,
};

enum HdcCommand {
    // core commands types
    CMD_KERNEL_HELP = 0,
    CMD_KERNEL_HANDSHAKE,
    CMD_KERNEL_CHANNEL_CLOSE,
    CMD_KERNEL_TARGET_DISCOVER = 4,
    CMD_KERNEL_TARGET_LIST,
    CMD_KERNEL_TARGET_ANY,
    CMD_KERNEL_TARGET_CONNECT,
    CMD_KERNEL_TARGET_DISCONNECT,
    CMD_KERNEL_ECHO,
    CMD_KERNEL_ECHO_RAW,
    CMD_KERNEL_ENABLE_KEEPALIVE,
    CMD_KERNEL_WAKEUP_SLAVETASK,
    CMD_CHECK_SERVER,
    CMD_CHECK_DEVICE,
    CMD_WAIT_FOR,
    CMD_SERVER_KILL,  // channel kill, not use
    CMD_SERVICE_START = 17,
    CMD_SSL_HANDSHAKE = 20,
    // One-pass simple commands
    CMD_UNITY_COMMAND_HEAD = 1000,  // not use
    CMD_UNITY_EXECUTE,
    CMD_UNITY_REMOUNT,
    CMD_UNITY_REBOOT,
    CMD_UNITY_RUNMODE,
    CMD_UNITY_HILOG,
    CMD_UNITY_ROOTRUN = 1007,
    CMD_JDWP_LIST,
    CMD_JDWP_TRACK,
    CMD_UNITY_COMMAND_TAIL,  // not use
    // It will be separated from unity in the near future
    CMD_UNITY_BUGREPORT_INIT,
    CMD_UNITY_BUGREPORT_DATA,
    CMD_UNITY_EXECUTE_EX = 1200,    // need to check TAG_FEATURE_SHELL_OPT
    // Shell commands types
    CMD_SHELL_INIT = 2000,
    CMD_SHELL_DATA,
    // Forward commands types
    CMD_FORWARD_INIT = 2500,
    CMD_FORWARD_CHECK,
    CMD_FORWARD_CHECK_RESULT,
    CMD_FORWARD_ACTIVE_SLAVE,
    CMD_FORWARD_ACTIVE_MASTER,
    CMD_FORWARD_DATA,
    CMD_FORWARD_FREE_CONTEXT,
    CMD_FORWARD_LIST,
    CMD_FORWARD_REMOVE,
    CMD_FORWARD_SUCCESS,
    // File commands
    CMD_FILE_INIT = 3000,
    CMD_FILE_CHECK,
    CMD_FILE_BEGIN,
    CMD_FILE_DATA,
    CMD_FILE_FINISH,
    CMD_APP_SIDELOAD,
    CMD_FILE_MODE,
    CMD_DIR_MODE,
    // App commands
    CMD_APP_INIT = 3500,
    CMD_APP_CHECK,
    CMD_APP_BEGIN,
    CMD_APP_DATA,
    CMD_APP_FINISH,
    CMD_APP_UNINSTALL,

    // deprecated, remove later
    CMD_UNITY_JPID = CMD_JDWP_LIST,

    // Flashd commands
    CMD_FLASHD_UPDATE_INIT = 4000,
    CMD_FLASHD_FLASH_INIT,
    CMD_FLASHD_CHECK,
    CMD_FLASHD_BEGIN,
    CMD_FLASHD_DATA,
    CMD_FLASHD_FINISH,
    CMD_FLASHD_ERASE,
    CMD_FLASHD_FORMAT,
    CMD_FLASHD_PROGRESS,

    //Heartbeat commands
    CMD_HEARTBEAT_MSG = 5000,
};

enum UsbProtocolOption {
    USB_OPTION_HEADER = 1,
    USB_OPTION_RESET = 2,
    USB_OPTION_RESERVE4 = 4,
    USB_OPTION_RESERVE8 = 8,
    USB_OPTION_RESERVE16 = 16,
};

enum ShellExecuteTag {
    TAG_SHELL_CMD = 0x00000000,
    TAG_SHELL_BUNDLE = 0x00000001,
    TAG_SHELL_DEFAULT = 0xFFFFFFFF,
};
}
#endif