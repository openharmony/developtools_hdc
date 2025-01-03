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
#ifndef DEFINE_PLUS_H
#define DEFINE_PLUS_H

#include <sstream>
#include <thread>
#ifdef HDC_TRACE
#include "hitrace_meter.h"
#endif
#include "define_enum.h"

namespace Hdc {
    
static string MaskString(const string &str)
{
    if (str.empty()) {
        return str;
    }
    size_t len = str.length();
    if (len <= 6) {  // 6: 当字符串长度小于等于6时，只保留首尾各一个字符, 掩码的个数为字符的长度
        return std::string(1, str.front()) + std::string(len, '*') + std::string(1, str.back());
    } else {
        // 3, 6: 对于较长的字符串，保留首尾各三个字符，掩码的个数为6
        return str.substr(0, 3) + std::string(6, '*') + str.substr(len - 3)  + "(L:" + std::to_string(len) + ")";
    }
}

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(exp) ({         \
    __typeof__(exp) _rc;                   \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && errno == EINTR); \
    _rc; })
#endif

// hitrace will increase ipc thread to upper hdcd memory
// IPC function SetMaxWorkThreadNum can limit thread num, default 16 threads
#ifdef HDC_TRACE_TEST // default close, change to open by HDC_TRACE
#define StartTracePoint(value) StartTrace(HITRACE_TAG_HDCD, value)
#define FinishTracePoint()     FinishTrace(HITRACE_TAG_HDCD)
#define StartTraceScope(value) HITRACE_METER_NAME(HITRACE_TAG_HDCD, value)
#else
#define StartTracePoint(value)
#define FinishTracePoint()
#define StartTraceScope(value)
#endif

// ################################### struct define ###################################
#pragma pack(push)
#pragma pack(1)

struct USBHead {
    uint8_t flag[2];
    uint8_t option;
    uint32_t sessionId;
    uint32_t dataSize;
};

struct AsyncParam {
    void *context;    // context=hsession or hchannel
    uint32_t sid;     // sessionId/channelId
    void *thisClass;  // caller's class ptr
    uint16_t method;
    int dataSize;
    void *data;  // put it in the last
};

struct TaskInformation {
    uint8_t taskType;
    uint32_t sessionId;
    uint32_t channelId;
    bool hasInitial;
    bool taskStop;
    bool taskFree;
    bool serverOrDaemon; // true is server, false is daemon
    bool masterSlave;
    uv_loop_t *runLoop;
    void *taskClass;
    void *ownerSessionClass;
    uint32_t closeRetryCount;
    bool channelTask;
    void *channelClass;
    uint8_t debugRelease; // 0:allApp 1:debugApp 2:releaseApp
    bool isStableBuf;
    bool isCleared; // true: RemoveInstanceTask OP_CLEAR is called
};
using HTaskInfo = TaskInformation *;

#pragma pack(pop)

#ifdef HDC_HOST
struct HostUSBEndpoint {
    HostUSBEndpoint(uint32_t epBufSize)
    {
        endpoint = 0;
        sizeEpBuf = epBufSize;  // MAX_USBFFS_BULK
        transfer = libusb_alloc_transfer(0);
        isShutdown = true;
        isComplete = true;
        bulkInOut = false;
        buf = new (std::nothrow) uint8_t[sizeEpBuf];
        (void)memset_s(buf, sizeEpBuf, 0, sizeEpBuf);
    }
    ~HostUSBEndpoint()
    {
        libusb_free_transfer(transfer);
        delete[] buf;
    }
    uint8_t endpoint;
    uint8_t *buf;  // MAX_USBFFS_BULK
    bool isComplete;
    bool isShutdown;
    bool bulkInOut;  // true is bulkIn
    uint32_t sizeEpBuf;
    std::mutex mutexIo;
    std::mutex mutexCb;
    condition_variable cv;
    libusb_transfer *transfer;
};
#endif

struct HdcUSB {
#ifdef HDC_HOST
    libusb_context *ctxUSB = nullptr;  // child-use, main null
    libusb_device *device = nullptr;
    libusb_device_handle *devHandle = nullptr;
    uint16_t retryCount;
    uint8_t devId;
    uint8_t busId;
    uint8_t interfaceNumber;
    std::string serialNumber;
    std::string usbMountPoint;
    HostUSBEndpoint hostBulkIn;
    HostUSBEndpoint hostBulkOut;
    HdcUSB() : hostBulkIn(513 * 1024), hostBulkOut(512 * 1024) {} // 513: 512 + 1, 1024: 1KB
    // 512 * 1024 + 1024 = 513 * 1024, MAX_USBFFS_BULK: 512 * 1024

#else
    // usb accessory FunctionFS
    // USB main thread use, sub-thread disable, sub-thread uses the main thread USB handle
    int bulkOut;  // EP1 device recv
    bool isBulkOutClosing;
    int bulkIn;   // EP2 device send
    bool isBulkInClosing;
#endif
    uint32_t payloadSize;
    uint16_t wMaxPacketSizeSend;
    bool resetIO;  // if true, must break write and read,default false
    std::mutex lockDeviceHandle;
    std::mutex lockSendUsbBlock;
};
using HUSB = struct HdcUSB *;

#ifdef HDC_SUPPORT_UART
struct HdcUART {
#ifdef HDC_HOST
    std::string serialPort;
    std::thread readThread;
    uint16_t retryCount = 0;
#endif // HDC_HOST

#ifdef _WIN32
    OVERLAPPED ovWrite;
    OVERLAPPED ovRead;
    HANDLE devUartHandle = INVALID_HANDLE_VALUE;
#else
    // we also make this for daemon side
    int devUartHandle = -1;
#endif
    // if we want to cancel io (read thread exit)
    bool ioCancel = false;
    uint32_t dispatchedPackageIndex = 0;
    bool resetIO = false; // if true, must break write and read,default false
    uint64_t packageIndex = 0;
    std::atomic_size_t streamSize = 0; // for debug only
    HdcUART();
    ~HdcUART();
};
using HUART = struct HdcUART *;
#endif
struct HdcSessionStat {
    // bytes successed send to hSession->dataFd[STREAM_MAIN]
    std::atomic<uint64_t> dataSendBytes;
    // bytes successed read from hSession->dataPipe[STREAM_WORK]
    std::atomic<uint64_t> dataRecvBytes;
};

struct HdcSession {
    bool serverOrDaemon;  // instance of daemon or server, true is server, false is daemon
    bool handshakeOK;     // Is an expected peer side
    bool isDead;
    bool voteReset;
    bool isCheck = false;
    std::string connectKey;
    uint8_t connType;  // ConnType
    uint32_t sessionId;
    std::atomic<uint32_t> ref;
    uint8_t uvHandleRef;  // libuv handle ref -- just main thread now
    uint8_t uvChildRef;   // libuv handle ref -- just main thread now
    bool childCleared;
    std::map<uint32_t, HTaskInfo> *mapTask;
    // class ptr
    void *classInstance;  //  HdcSessionBase instance, HdcServer or HdcDaemon
    void *classModule;    //  Communicate module, TCP or USB instance,HdcDaemonUSB HdcDaemonTCP etc...
    // io cache
    int bufSize;         // total buffer size
    int availTailIndex;  // buffer available data size
    uint8_t *ioBuf;
    // auth
    std::list<void *> *listKey;  // rsa private or publickey list
    uint8_t authKeyIndex;
    std::string tokenRSA;  // SHA_DIGEST_LENGTH+1==21
    // child work
    uv_loop_t childLoop;  // run in work thread
    // pipe0 in main thread(hdc server mainloop), pipe1 in work thread
    uv_poll_t *pollHandle[2];  // control channel
    int ctrlFd[2];         // control channel socketpair
    // data channel(TCP with socket, USB with thread forward)
    uv_tcp_t dataPipe[2];
    int dataFd[2];           // data channel socketpair
    uv_tcp_t hChildWorkTCP;  // work channel，separate thread for server/daemon
    uv_os_sock_t fdChildWorkTCP;
    // usb handle
    HUSB hUSB;
#ifdef HDC_SUPPORT_UART
    HUART hUART = nullptr;
#endif
    // tcp handle
    uv_tcp_t hWorkTCP;
    uv_thread_t hWorkThread;
    uv_thread_t hWorkChildThread;
    std::mutex mapTaskMutex;
    AuthVerifyType verifyType;
    bool isSoftReset; // for daemon, Used to record whether a reset command has been received

    HdcSessionStat stat;
    std::string ToDebugString()
    {
        std::ostringstream oss;
        oss << "HdcSession [";
        oss << " serverOrDaemon:" << serverOrDaemon;
        oss << " sessionId:" << sessionId;
        oss << " handshakeOK:" << handshakeOK;
        oss << " connectKey:" << Hdc::MaskString(connectKey);
        oss << " connType:" << unsigned(connType);
        oss << " ]";
        return oss.str();
    }

    HdcSession():serverOrDaemon(false), handshakeOK(false), isDead(false), voteReset(false)
    {
        connectKey = "";
        connType = CONN_USB;
        sessionId = 0;
        ref = 0;
        uvHandleRef = 0;
        uvChildRef = 0;
        childCleared = false;
        mapTask = nullptr;
        classInstance = nullptr;
        classModule = nullptr;
        bufSize = 0;
        ioBuf = nullptr;
        availTailIndex = 0;
        listKey = nullptr;
        authKeyIndex = 0;
        tokenRSA = "";
        hUSB = nullptr;
        (void)memset_s(pollHandle, sizeof(pollHandle), 0, sizeof(pollHandle));
        (void)memset_s(ctrlFd, sizeof(ctrlFd), 0, sizeof(ctrlFd));
        (void)memset_s(dataFd, sizeof(dataFd), 0, sizeof(dataFd));
        (void)memset_s(&childLoop, sizeof(childLoop), 0, sizeof(childLoop));
        (void)memset_s(dataPipe, sizeof(dataPipe), 0, sizeof(dataPipe));
        (void)memset_s(&hChildWorkTCP, sizeof(hChildWorkTCP), 0, sizeof(hChildWorkTCP));
        (void)memset_s(&fdChildWorkTCP, sizeof(fdChildWorkTCP), 0, sizeof(fdChildWorkTCP));
        (void)memset_s(&stat, sizeof(stat), 0, sizeof(stat));
#ifdef HDC_SUPPORT_UART
        hUART = nullptr;
#endif
        verifyType = AuthVerifyType::RSA_3072_SHA512;
        isSoftReset = false;
    }

    ~HdcSession()
    {
        if (mapTask) {
            delete mapTask;
            mapTask = nullptr;
        }
        if (listKey) {
            delete listKey;
            listKey = nullptr;
        }
    }
};
using HSession = struct HdcSession *;

enum class RemoteType {
    REMOTE_NONE = 0,
    REMOTE_FILE = 1,
    REMOTE_APP = 2,
};

struct HdcChannel {
    void *clsChannel;  // ptr Class of serverForClient or client
    uint32_t channelId;
    std::string connectKey;
    uv_tcp_t hWorkTCP;  // work channel for client, forward channel for server
    uv_thread_t hWorkThread;
    uint8_t uvHandleRef = 0;  // libuv handle ref -- just main thread now
    bool handshakeOK;
    bool isDead;
    bool serverOrClient;  // true: server's channel, false: client's channel
    bool childCleared;
    bool interactiveShellMode;  // Is shell interactive mode
    bool keepAlive;             // channel will not auto-close by server
    std::atomic<uint32_t> ref;
    uint32_t targetSessionId;
    // child work
    uv_tcp_t hChildWorkTCP;  // work channel for server, no use in client
    uv_os_sock_t fdChildWorkTCP;
    // read io cache
    int bufSize;         // total buffer size
    int availTailIndex;  // buffer available data size
    uint8_t *ioBuf;
    // std
    uv_tty_t stdinTty;
    uv_tty_t stdoutTty;
    char bufStd[128];
    bool isCheck = false;
    std::string key;
    RemoteType remote = RemoteType::REMOTE_NONE;
    bool fromClient = false;
    bool connectLocalDevice = false;
    bool isStableBuf = false;
    std::atomic<uint32_t> writeFailedTimes;
};
using HChannel = struct HdcChannel *;

struct HdcDaemonInformation {
    uint8_t connType;
    uint8_t connStatus;
    std::string connectKey;
    std::string usbMountPoint;
    std::string devName;
    HSession hSession;
    std::string version;
    std::string emgmsg;
    std::string daemonAuthStatus;
    std::map<std::string, std::string> daemonFeature;
};
using HDaemonInfo = struct HdcDaemonInformation *;

struct HdcForwardInformation {
    std::string taskString;
    bool forwardDirection;  // true for forward, false is reverse;
    uint32_t sessionId;
    uint32_t channelId;
    std::string connectKey;
};
using HForwardInfo = struct HdcForwardInformation *;

struct HdcSessionInfo {
    uint32_t sessionId = 0;
    HSession hSession = nullptr;

    // class ptr
    void *classInstance = nullptr;  //  HdcSessionBase instance, HdcServer or HdcDaemon
    void *classModule = nullptr;    //  Communicate module, TCP or USB instance,HdcDaemonUSB HdcDaemonTCP etc...
};
using HSessionInfo = struct HdcSessionInfo *;
}
#endif
