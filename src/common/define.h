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
#ifndef HDC_DEFINE_H
#define HDC_DEFINE_H
#include "define_plus.h"
#include "log.h"

namespace Hdc {
// ############################## config #######################################
#ifndef USE_CONFIG_UV_THREADS
constexpr size_t SIZE_THREAD_POOL = 16;
#endif
constexpr size_t SIZE_THREAD_POOL_MIN = 16;
constexpr size_t SIZE_THREAD_POOL_MAX = 256;
constexpr uint8_t GLOBAL_TIMEOUT = 30;
constexpr uint16_t DEFAULT_PORT = 8710;
#ifdef FEATURE_HOST_LOG_COMPRESS
constexpr uint16_t MAX_LOG_FILE_COUNT = 300;
#else
constexpr uint16_t MAX_LOG_FILE_COUNT = 30;
#endif
constexpr uint64_t MAX_LOG_FILE_SIZE = static_cast<uint64_t>(100) * 1024 * 1024; // 100MB
constexpr uint64_t MAX_LOG_DIR_SIZE = static_cast<uint64_t>(3) * 1024 * 1024 * 1024; // 3GB
constexpr bool ENABLE_IO_CHECKSUM = false;
const string IPV4_MAPPING_PREFIX = "::ffff:";
const string DEFAULT_SERVER_ADDR_IP = "::ffff:127.0.0.1";
const string DEFAULT_SERVER_ADDR = "::ffff:127.0.0.1:8710";
const string ENV_SERVER_PORT = "OHOS_HDC_SERVER_PORT";
const string ENV_SERVER_LOG = "OHOS_HDC_LOG_LEVEL";
const string ENV_SERVER_LOG_LIMIT = "OHOS_HDC_LOG_LIMIT";
const string ENV_SERVER_HEARTBEAT = "OHOS_HDC_HEARTBEAT"; // 1: turn off heartbeat
const string ENV_ENCRYPT_CHANNEL = "OHOS_HDC_ENCRYPT_CHANNEL"; // 1: turn on encrypt channel
const string STR_PSK_IDENTITY = "Client_identity";
const string TLS_AES_128_GCM_SHA256 = "TLS_AES_128_GCM_SHA256";
#ifdef __OHOS__
const string UDS_PATH = "/data/hdc/hdc_debug/hdc_server";
const string UDS_STR = "uds";
#endif

// ################################ macro define ###################################
constexpr uint8_t MINOR_TIMEOUT = 5;
constexpr uint8_t MILL_SECONDS = 10;
constexpr uint8_t WAIT_SECONDS = 30;
constexpr uint8_t DWORD_SERIALIZE_SIZE = 4;
constexpr uint8_t OFFSET = 2;
constexpr uint8_t CMD_ARG1_COUNT = 2;
constexpr uint8_t COUNT = 10;
constexpr uint8_t STREAM_MAIN = 0;  // work at main thread
constexpr uint8_t STREAM_WORK = 1;  // work at work thread
constexpr uint8_t STREAM_SIZE = 2;
constexpr uint8_t FEATURE_FLAG_MAX_SIZE = 8;
constexpr uint16_t TIME_BUF_SIZE = 32;
constexpr uint16_t BUF_SIZE_SSL_HEAD = 22; // 22: SSL head size
constexpr uint16_t BUF_SIZE_PSK = 32; // 32: PreShared Key size
constexpr uint16_t BUF_SIZE_PSK_ENCRYPTED = 512;
constexpr uint16_t BUF_SIZE_MICRO = 16;
constexpr uint16_t BUF_SIZE_TINY = 64;
constexpr uint16_t BUF_SIZE_SMALL = 256;
constexpr uint16_t BUF_SIZE_MEDIUM = 512;
constexpr uint16_t BUF_SIZE_DEFAULT = 1024;
constexpr uint16_t BUF_SIZE_DEFAULT2 = BUF_SIZE_DEFAULT * 2;
constexpr uint16_t BUF_SIZE_DEFAULT4 = BUF_SIZE_DEFAULT * 4;
constexpr uint32_t BUF_SIZE_DEFAULT16 = BUF_SIZE_DEFAULT * 16;
constexpr uint16_t MAX_IP_PORT = 65535;
constexpr uint16_t MAX_CONNECTKEY_SIZE = 32;  // usb sn/tcp ipport
constexpr uint16_t TIME_BASE = 1000;          // time unit conversion base value
constexpr uint16_t UV_DEFAULT_INTERVAL = 250;  // ms
constexpr uint16_t VER_PROTOCOL = 0x01;
constexpr uint16_t MAX_PACKET_SIZE_HISPEED = 512;
constexpr uint16_t DEVICE_CHECK_INTERVAL = 3000;  // ms
constexpr uint32_t MAX_SIZE_IOBUF = 511 * 1024; // 511KB, large file transfer speed up
constexpr uint32_t MAX_USBFFS_BULK = 512 * 1024; // 512KB, large file transfer speed up
constexpr uint32_t MAX_SIZE_IOBUF_STABLE = 60 * 1024; // 60KB, compatible with previous version
constexpr uint32_t MAX_USBFFS_BULK_STABLE = 61 * 1024; // 61KB, compatible with provious version
constexpr uint16_t MAX_LOG_TIMER = 20;
constexpr uint16_t MAX_LOG_REPRINT_COUNT = 5;
constexpr double DECODE_SCALE = 0.75;
constexpr uint16_t LAST_EQUAL_NUM = 2;
constexpr uint16_t UV_START_TIMEOUT = 10;
constexpr uint16_t UV_START_REPEAT = 10;
constexpr uint16_t UV_LISTEN_LBACKOG = 4;
constexpr uint16_t BUF_EXTEND_SIZE = 4;
constexpr uint16_t BUF_MULTIPLE = 2;
constexpr uint16_t EXPECTED_LEN = 9;
constexpr uint16_t MAX_APP_PARAM_SIZE = 512;
constexpr uint16_t MIN_APP_PARAM_SIZE = 4;
constexpr uint16_t FORWORD_PORT_RM_BUF_SIZE = 9;
constexpr uint16_t FORWORD_PORT_OTHER_BUF_SIZE = 6;
constexpr uint16_t CMD_REMOTE_SIZE = 8;
constexpr uint16_t PORT_MAX_LEN = 5;
constexpr uint16_t START_SERVER_FOR_CLIENT_TIME = 300;
constexpr uint16_t START_CMD_WAIT_TIME = 3000; // 3 seconds, for client wait for server start
constexpr uint16_t CMD_FILE_PENULT_PARAM = 2;
constexpr uint16_t BUNDLE_MIN_SIZE = 7;
constexpr uint16_t BUNDLE_MAX_SIZE = 128;
constexpr uint16_t HEARTBEAT_INTERVAL = 5000; // 5 seconds
constexpr uint16_t SSL_HANDSHAKE_FINISHED_WAIT_TIME = 300; // 300 ms
#ifdef HDC_HOST
constexpr uint16_t MAX_DELETED_SESSION_ID_RECORD_COUNT = 32;
#else
constexpr uint16_t MAX_DELETED_SESSION_ID_RECORD_COUNT = 10;
#endif
constexpr uint16_t TCP_CONNECT_MAX_RETRY_COUNT = 6;
constexpr uint16_t TCP_CONNECT_RETRY_TIME_MS = 500;

// the maximum time (in milliseconds) to drop USB channel data for new session
constexpr uint16_t NEW_SESSION_DROP_USB_DATA_TIME_MAX_MS = 1000;

// double-word(hex)=[0]major[1][2]minor[3][4]version[5]fix(a-p)[6][7]reserve
// |----------------------------------------------------------------|
// | 31-28 | 27-24 | 23-20 | 19-16 | 15-12 | 11-08 |     07-00      |
// |----------------------------------------------------------------|
// | major |reserve| minor |reserve|version|  fix  |   reserve      |
// |----------------------------------------------------------------|
// 0x30200200 is 3.2.0c
constexpr uint32_t HDC_VERSION_NUMBER = 0x30200200;
constexpr uint32_t HDC_BUF_MAX_BYTES = INT_MAX;
#ifdef HDC_HOST
constexpr uint32_t HOST_SOCKETPAIR_SIZE = 1024 * 1024;
#endif
constexpr uint32_t HDC_SOCKETPAIR_SIZE = MAX_SIZE_IOBUF * 2;
// "\f" asicc is 12
const string HDC_HOST_DAEMON_BUF_SEPARATOR = "\f";
constexpr int32_t RSA_KEY_BITS = 3072;

const string WHITE_SPACES = " \t\n\r";
const string UT_TMP_PATH = "/tmp/hdc-ut";
const string DEBUG_BUNDLE_PATH = "/mnt/debug/100/debug_hap/";
const string LOG_FILE_NAME = "hdc.log";
const string LOG_FILE_NAME_PREFIX = "hdc-";
const string LOG_FILE_SUFFIX = ".log";
const string LOG_FILE_COMPRESS_SUFFIX = ".tgz";
const string LOG_COMPRESS_TOOL_NAME = "tar";
const string LOG_CACHE_NAME = ".hdc.cache.log";
const string LOG_DIR_NAME = "hdc_logs";
const string LOG_COMPRESS_TOOL_PARAMS = "czfp";
const string SERVER_NAME = "HDCServer";
const string STRING_EMPTY = "";
const string HANDSHAKE_MESSAGE = "OHOS HDC";  // sep not char '-', not more than 11 bytes
const string HANDSHAKE_FAILED = "HS FAILED";
const string PACKET_FLAG = "HW";              // Must be 2 bytes, representing the abbreviation for hdc work
const string EMPTY_ECHO = "[Empty]";
const string MESSAGE_INFO = "[Info]";
const string MESSAGE_FAIL = "[Fail]";
const string MESSAGE_SUCCESS = "[Success]";
const char HUGE_BUF_TAG = 'H';                // support huge buffer
const size_t BANNER_FEATURE_TAG_OFFSET = 11;
const char WAIT_DEVICE_TAG = 'W';
const size_t WAIT_TAG_OFFSET = 11;
#ifdef HOST_OHOS
const size_t SERVICE_KILL_OFFSET = 10;
const char SERVICE_KILL_TAG = 'K';
#endif
// input command
const string CMDSTR_SOFTWARE_VERSION = "version";
const string CMDSTR_SOFTWARE_HELP = "help";
const string CMDSTR_TARGET_DISCOVER = "discover";
const string CMDSTR_SERVICE_START = "start";
const string CMDSTR_SERVICE_KILL = "kill";
const string CMDSTR_GENERATE_KEY = "keygen";
const string CMDSTR_LIST_TARGETS = "list targets";
const string CMDSTR_CHECK_SERVER = "checkserver";
const string CMDSTR_CHECK_DEVICE = "checkdevice";
const string CMDSTR_WAIT_FOR = "wait";
const string CMDSTR_CONNECT_TARGET = "tconn";
const string CMDSTR_CONNECT_ANY = "any";
const string CMDSTR_SHELL = "shell";
const string CMDSTR_SHELL_EX = "shell -";
const string CMDSTR_TARGET_REBOOT = "target boot";
const string CMDSTR_TARGET_MOUNT = "target mount";
const string CMDSTR_STARTUP_MODE = "smode";
const string CMDSTR_TARGET_MODE = "tmode";
const string CMDSTR_BUGREPORT = "bugreport";
const string CMDSTR_HILOG = "hilog";
const string CMDSTR_TMODE_USB = "usb";
#ifdef _WIN32
const string HILOG_NAME = "hilog.exe";
const string SPLIT = "\\";
const string LOG_COMPRESS_TOOL_BIN_WIN32 = "C:\\Windows\\System32\\tar.exe";
#else
const string LOG_COMPRESS_TOOL_BIN_UNIX = "tar";
#endif
#ifdef HDC_SUPPORT_UART
const string CMDSTR_TMODE_UART = "uart";
const string UART_HDC_NODE = "/dev/ttyS4";
const string CONSOLE_ACTIVE_NODE = "/sys/class/tty/console/active";
constexpr int UART_IO_WAIT_TIME_100 = 100;
constexpr int UART_IO_WAIT_TIME = 1000;
constexpr uint16_t MAX_UART_SIZE_IOBUF = 4096; // MAX_SIZE_IOBUF;
#endif
const string CMDSTR_TMODE_TCP = "tcp";
#ifdef HDC_EMULATOR
const string CMDSTR_TMODE_BRIDGE = "bridge";
#endif
const string CMDSTR_FILE_SEND = "file send";
const string CMDSTR_FILE_RECV = "file recv";
const string CMDSTR_REMOTE_PARAMETER = "remote";
const string CMDSTR_FORWARD_FPORT = "fport";
const string CMDSTR_FORWARD_RPORT = "rport";
const string CMDSTR_APP_INSTALL = "install";
const string CMDSTR_APP_UNINSTALL = "uninstall";
const string CMDSTR_APP_SIDELOAD = "sideload";
const string CMDSTR_LIST_JDWP = "jpid";
const string CMDSTR_TRACK_JDWP = "track-jpid";
const string CMDSTR_INNER_ENABLE_KEEPALIVE = "alive";
const string CMDSTR_FLASHD_UPDATE = "update";
const string CMDSTR_FLASHD_FLASH = "flash";
const string CMDSTR_FLASHD_ERASE = "erase";
const string CMDSTR_FLASHD_FORMAT = "format";
const string STR_FEATURE_ENABLE = "enable";
const string STR_FEATURE_DISABLE = "disable";
// ################################ Error Message ###################################
const string TERMINAL_HDC_PROCESS_FAILED = "[E002101]:Terminal hdc process failed, "\
    "please terminal the hdc process in the task manager first.";

// ################################ Macro define ###################################
#ifdef _WIN32
#define hdc_strerrno(buf) \
        char buf[1024] = { 0 }; \
        strerror_s(buf, 1024, errno)
#else
#define hdc_strerrno(buf)  \
        char buf[1024] = { 0 };  \
        strerror_r(errno, buf, 1024)
#endif
const string ENV_SERVER_CMD_LOG = "OHOS_HDC_CMD_RECORD";
const string CMD_LOG_FILE_NAME = "hdc-cmd.log";
const string CMD_LOG_DIR_NAME = "hdc_cmd";
const string CMD_LOG_FILE_TYPE = ".log";
const string CMD_LOG_COMPRESS_FILE_NAME_PREFIX = "hdc-cmd-";
const string CMD_LOG_COMPRESS_FILE_NAME_SUFFIX = ".tgz";
constexpr uint16_t MAX_COMPRESS_LOG_FILE_COUNT = 47;
constexpr uint64_t MAX_COMPRESS_LOG_FILE_SIZE = (50) * 1024 * 1024; // 50MB
constexpr size_t MAX_SAVE_CMD_LOG_TO_FILE_CYCLE = 10;
constexpr size_t MAX_SAVE_CMD_LOG_TO_FILE_COUNTS = 1500;
constexpr size_t MAX_PROCESS_TIMEOUT = 120; // 120 seconds
constexpr int64_t MAX_FREEZE_TIME = 30 * TIME_BASE;       // 30 seconds
}  // namespace Hdc
#endif  // HDC_DEFINE_H
