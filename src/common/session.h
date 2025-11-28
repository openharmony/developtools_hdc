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
#ifndef HDC_SESSION_H
#define HDC_SESSION_H
#include <shared_mutex>
#include <sstream>
#include "common.h"
#include "uv_status.h"

namespace Hdc {
enum TaskType { TYPE_UNITY, TYPE_SHELL, TASK_FILE, TASK_FORWARD, TASK_APP, TASK_FLASHD };

class HdcSessionBase {
public:
    enum AuthType { AUTH_NONE, AUTH_TOKEN, AUTH_SIGNATURE, AUTH_PUBLICKEY, AUTH_OK, AUTH_FAIL, AUTH_SSL_TLS_PSK };
    struct SessionHandShake {
        string banner; // must first index
        // auth none
        uint8_t authType;
        uint32_t sessionId;
        string connectKey;
        string buf;
        string version;
        std::string ToDebugString()
        {
            std::ostringstream oss;
            oss << "SessionHandShake [";
            oss << " banner:" << banner;
            oss << " sessionId:" << sessionId;
            oss << " authType:" << unsigned(authType);
            oss << " connectKey:" << Hdc::MaskString(connectKey);
            oss << " version:" << version;
            oss << "]";
            return oss.str();
        }
    };

    struct CtrlStruct {
        InnerCtrlCommand command;
        uint32_t channelId;
        uint8_t dataSize;
        uint8_t data[BUF_SIZE_MICRO];
    };
    struct PayloadProtect {  // reserve for encrypt and decrypt
        uint32_t channelId;
        uint32_t commandFlag;
        uint8_t checkSum;  // enable it will be lose about 20% speed
        uint8_t vCode;
    };
    struct HeartbeatMsg {
        uint64_t heartbeatCount;
        string reserved;
    };

    HdcSessionBase(bool serverOrDaemonIn, size_t uvThreadSize = SIZE_THREAD_POOL);
    virtual ~HdcSessionBase();
    virtual void AttachChannel(HSession /* hSession */, const uint32_t /* channelId */)
    {
    }
    virtual void DeatchChannel(HSession /* hSession */, const uint32_t /* channelId */)
    {
    }
    virtual void NotifyInstanceSessionFree(HSession /* hSession */, bool /* freeOrClear */)
    {
    }
    virtual bool RedirectToTask(HTaskInfo /* hTaskInfo */, HSession /* hSession */, const uint32_t /* channelId */,
                                const uint16_t /* command */, uint8_t* /* payload */, const int /* payloadSize */)
    {
        return true;
    }
    // Thread security interface for global stop programs
    void PostStopInstanceMessage(bool restart = false);
    void ReMainLoopForInstanceClear();
    // server, Two parameters in front of call can be empty
    void LogMsg(const uint32_t sessionId, const uint32_t channelId, MessageLevel level, const char *msg, ...);
    static void AllocCallback(uv_handle_t *handle, size_t sizeWanted, uv_buf_t *buf);
    static void MainAsyncCallback(uv_async_t *handle);
    static void FinishWriteSessionTCP(uv_write_t *req, int status);
    static void SessionWorkThread(uv_work_t *arg);
    static void ReadCtrlFromSession(uv_poll_t *poll, int status, int events);
    static void ReadCtrlFromMain(uv_poll_t *poll, int status, int events);
    static void ParsePeerSupportFeatures(HSession &hSession, std::map<std::string, std::string> &tlvMap);
    HSession QueryUSBDeviceRegister(void *pDev, uint8_t busIDIn, uint8_t devIDIn);
    virtual HSession MallocSession(bool serverOrDaemon, const ConnType connType, void *classModule,
                                   uint32_t sessionId = 0);
    virtual void FreeSession(const uint32_t sessionId);
#ifdef HDC_HOST
    void PrintAllSessionConnection(const uint32_t sessionId);
#else
    void AdminUsbSession();
#endif
    void WorkerPendding();
    int OnRead(HSession hSession, uint8_t *bufPtr, const int bufLen);
    int Send(const uint32_t sessionId, const uint32_t channelId, const uint16_t commandFlag, const uint8_t *data,
             const int dataSize);
    int SendByProtocol(HSession hSession, uint8_t *bufPtr, const int bufLen, bool echo = false);
    virtual HSession AdminSession(const uint8_t op, const uint32_t sessionId, HSession hInput);
    virtual int FetchIOBuf(HSession hSession, uint8_t *ioBuf, int read);
#ifdef HDC_SUPPORT_ENCRYPT_TCP
    virtual int FetchIOBuf(HSession hSession, uint8_t *ioBuf, int read, bool isEncrypted);
#endif
    virtual void PushAsyncMessage(const uint32_t sessionId, const uint8_t method, const void *data, const int dataSize);
    HTaskInfo AdminTask(const uint8_t op, HSession hSession, const uint32_t channelId, HTaskInfo hInput);
    bool DispatchTaskData(HSession hSession, const uint32_t channelId, const uint16_t command, uint8_t *payload,
                          int payloadSize);
    void EnumUSBDeviceRegister(void (*pCallBack)(HSession hSession));
#ifdef HDC_SUPPORT_UART
    using UartKickoutZombie = const std::function<void(HSession hSession)>;
    virtual void EnumUARTDeviceRegister(UartKickoutZombie);
#endif
    void ClearOwnTasks(HSession hSession, const uint32_t channelIDInput);
    virtual bool FetchCommand(HSession /* hSession */, const uint32_t /* channelId */,
                              const uint16_t /* command */, uint8_t* /* payload */, int /* payloadSize */)
    {
        return true;
    }
    virtual bool ServerCommand(const uint32_t /* sessionId */, const uint32_t /* channelId */,
                               const uint16_t /* command */, uint8_t* /* bufPtr */, const int /* size */)
    {
        return true;
    }
    virtual bool RemoveInstanceTask(const uint8_t /* op */, HTaskInfo /* hTask */)
    {
        return true;
    }
    bool WantRestart()
    {
        return wantRestart;
    }
    static vector<uint8_t> BuildCtrlString(InnerCtrlCommand command, uint32_t channelId, uint8_t *data, int dataSize);
    uv_loop_t loopMain;
    LoopStatus loopMainStatus;
    bool serverOrDaemon;
    uv_async_t asyncMainLoop;
    uv_rwlock_t mainAsync;
    list<void *> lstMainThreadOP;
    void *ctxUSB;

protected:
    struct PayloadHead {
        uint8_t flag[2];
        uint8_t reserve[2];  // encrypt'flag or others options
        uint8_t protocolVer;
        uint16_t headSize;
        uint32_t dataSize;
    } __attribute__((packed));
    void ClearSessions();
#ifdef HDC_HOST
    void PrintSession(const uint32_t sessionId);
#endif
    HSession VoteReset(const uint32_t sessionId);
    virtual void JdwpNewFileDescriptor(const uint8_t* /* buf */, const int /* bytesIO */)
    {
    }
    // must be define in haderfile, cannot in cpp file
    template<class T>
    bool TaskCommandDispatch(HTaskInfo hTaskInfo, uint8_t taskType, const uint16_t command, uint8_t *payload,
                             const int payloadSize)
    {
        StartTraceScope("HdcSessionBase::TaskCommandDispatch");
        bool ret = true;
        T *ptrTask = nullptr;
        if (!hTaskInfo->hasInitial) {
            hTaskInfo->taskType = taskType;
            ptrTask = new(std::nothrow) T(hTaskInfo);
            if (ptrTask == nullptr) {
                return false;
            }
            hTaskInfo->hasInitial = true;
            hTaskInfo->taskClass = ptrTask;
        } else {
            ptrTask = static_cast<T *>(hTaskInfo->taskClass);
        }
        if (!ptrTask->CommandDispatch(command, payload, payloadSize)) {
            ptrTask->TaskFinish();
        }
        return ret;
    }
    template<class T> bool DoTaskRemove(HTaskInfo hTaskInfo, const uint8_t op)
    {
        T *ptrTask = static_cast<T *>(hTaskInfo->taskClass);
        if (ptrTask == nullptr) {
            return true;
        }
        if (op == OP_CLEAR) {
            ptrTask->StopTask();
        } else if (op == OP_REMOVE) {
            if (!ptrTask->ReadyForRelease()) {
                return false;
            }
            delete ptrTask;
        }
        return true;
    }
    bool wantRestart;

private:
    virtual void ClearInstanceResource()
    {
    }
    int DecryptPayload(HSession hSession, PayloadHead *payloadHeadBe, uint8_t *encBuf);
    bool DispatchMainThreadCommand(HSession hSession, const CtrlStruct *ctrl);
    bool DispatchSessionThreadCommand(const uint8_t *baseBuf, const int bytesIO);
    void BeginRemoveTask(HTaskInfo hTask);
    bool TryRemoveTask(HTaskInfo hTask);
    void ReChildLoopForSessionClear(HSession hSession);
    void FreeSessionContinue(HSession hSession);
    static void FreeSessionFinally(uv_idle_t *handle);
    static void AsyncMainLoopTask(uv_idle_t *handle);
    static void FreeSessionOpeate(uv_timer_t *handle);
    static void SendHeartbeatMsg(uv_timer_t *handle);
    int MallocSessionByConnectType(HSession hSession);
    void FreeSessionByConnectType(HSession hSession);
    bool WorkThreadStartSession(HSession hSession);
    void WorkThreadInitSession(HSession hSession, SessionHandShake &handshake);
    uint32_t GetSessionPseudoUid();
    bool NeedNewTaskInfo(const uint16_t command, bool &masterTask);
    void DumpTasksInfo(map<uint32_t, HTaskInfo> &mapTask);
    void StartHeartbeatWork(HSession hSession);
    void SetFeature(SessionHandShake &handshake, const uint8_t connType);
    void StopHeartbeatWork(HSession hSession);
    static void TaskClassDeleteRetry(uv_timer_t *handle);
    std::string GetOs();
    map<uint32_t, HSession> mapSession;
    uv_rwlock_t lockMapSession;
    std::atomic<uint32_t> sessionRef = 0;
    const uint8_t payloadProtectStaticVcode = 0x09;
    uv_thread_t threadSessionMain;
    size_t threadPoolCount;
    std::atomic<uint32_t> taskCount = 0;
    uint64_t freeBegin = 0;
};
}  // namespace Hdc
#endif
