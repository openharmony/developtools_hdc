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

#include "connect_validation.h"
#include "session.h"
#ifndef TEST_HASH
#include "hdc_hash_gen.h"
#endif
#include "hdc_statistic_reporter.h"
#include "serial_struct.h"

namespace Hdc {
HdcSessionBase::HdcSessionBase(bool serverOrDaemonIn, size_t uvThreadSize) : loopMainStatus(&loopMain, "MainLoop")
{
    // print version pid
    WRITE_LOG(LOG_INFO, "Program running. %s Pid:%u", Base::GetVersion().c_str(), getpid());
    // server/daemon common initialization code
    if (uvThreadSize < SIZE_THREAD_POOL_MIN) {
        uvThreadSize = SIZE_THREAD_POOL_MIN;
    } else if (uvThreadSize > SIZE_THREAD_POOL_MAX) {
        uvThreadSize = SIZE_THREAD_POOL_MAX;
    }
    threadPoolCount = uvThreadSize;
    WRITE_LOG(LOG_INFO, "set UV_THREADPOOL_SIZE:%zu", threadPoolCount);
    string uvThreadEnv("UV_THREADPOOL_SIZE");
    string uvThreadVal = std::to_string(threadPoolCount);
#ifdef _WIN32
    uvThreadEnv += "=";
    uvThreadEnv += uvThreadVal;
    _putenv(uvThreadEnv.c_str());
#else
    setenv(uvThreadEnv.c_str(), uvThreadVal.c_str(), 1);
#endif
    uv_loop_init(&loopMain);
    WRITE_LOG(LOG_DEBUG, "loopMain init");
    uv_rwlock_init(&mainAsync);
#ifndef FUZZ_TEST
    loopMainStatus.StartReportTimer();
    uv_async_init(&loopMain, &asyncMainLoop, MainAsyncCallback);
#endif
    uv_rwlock_init(&lockMapSession);
    serverOrDaemon = serverOrDaemonIn;
    ctxUSB = nullptr;
    wantRestart = false;
    threadSessionMain = uv_thread_self();

#ifdef HDC_HOST
    if (serverOrDaemon) {
        if (libusb_init((libusb_context **)&ctxUSB) != 0) {
            ctxUSB = nullptr;
            WRITE_LOG(LOG_FATAL, "libusb_init failed ctxUSB is nullptr");
        }
    }
#endif
    HdcStatisticReporter::GetInstance().Schedule(&loopMain);
}

HdcSessionBase::~HdcSessionBase()
{
#ifndef FUZZ_TEST
    Base::TryCloseHandle((uv_handle_t *)&asyncMainLoop);
#endif
    uv_loop_close(&loopMain);
    // clear base
    uv_rwlock_destroy(&mainAsync);
    uv_rwlock_destroy(&lockMapSession);
#ifdef HDC_HOST
    if (serverOrDaemon and ctxUSB != nullptr) {
        libusb_exit((libusb_context *)ctxUSB);
    }
#endif
    WRITE_LOG(LOG_WARN, "~HdcSessionBase free sessionRef:%u instance:%s", uint32_t(sessionRef),
              serverOrDaemon ? "server" : "daemon");
}

// remove step2
bool HdcSessionBase::TryRemoveTask(HTaskInfo hTask)
{
    if (hTask->taskFree) {
        WRITE_LOG(LOG_WARN, "TryRemoveTask channelId:%u", hTask->channelId);
        return true;
    }
    bool ret = RemoveInstanceTask(OP_REMOVE, hTask);
    if (ret) {
        hTask->taskFree = true;
    } else {
        // This is used to check that the memory cannot be cleaned up. If the memory cannot be released, break point
        // here to see which task has not been released
        // print task clear
    }
    return ret;
}

void HdcSessionBase::TaskClassDeleteRetry(uv_timer_t *handle)
{
    StartTraceScope("HdcSessionBase::BeginRemoveTask taskClassDeleteRetry");
    HTaskInfo hTask = (HTaskInfo)handle->data;
    HdcSessionBase *thisClass = (HdcSessionBase *)hTask->ownerSessionClass;
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hTask->sessionId);
    if (hTask->isCleared == false) {
        hTask->isCleared = true;
        WRITE_LOG(LOG_WARN, "taskClassDeleteRetry start clear task, taskType:%d cid:%u sid:%s",
            hTask->taskType, hTask->channelId, sessionIdMaskStr.c_str());
        bool ret = thisClass->RemoveInstanceTask(OP_CLEAR, hTask);
        if (!ret) {
            WRITE_LOG(LOG_WARN, "taskClassDeleteRetry RemoveInstanceTask return false taskType:%d cid:%u sid:%s",
                hTask->taskType, hTask->channelId, sessionIdMaskStr.c_str());
        }
    }

    constexpr uint32_t count = 1000;
    if (hTask->closeRetryCount == 0 || hTask->closeRetryCount > count) {
        WRITE_LOG(LOG_DEBUG, "TaskDelay task remove retry count %d/%d, taskType:%d channelId:%u, sessionId:%s",
            hTask->closeRetryCount, GLOBAL_TIMEOUT, hTask->taskType, hTask->channelId, sessionIdMaskStr.c_str());
        hTask->closeRetryCount = 1;
    }
    hTask->closeRetryCount++;
    if (!thisClass->TryRemoveTask(hTask)) {
        WRITE_LOG(LOG_WARN, "TaskDelay TryRemoveTask false channelId:%u", hTask->channelId);
        return;
    }
    WRITE_LOG(LOG_WARN, "TaskDelay task remove finish, channelId:%u", hTask->channelId);
    if (hTask->taskType == TASK_FORWARD) {
        HdcStatisticReporter::GetInstance().IncrCommandInfo(STATISTIC_ITEM::FPORT_RM_COUNT);
    }
    if (hTask != nullptr) {
        delete hTask;
        hTask = nullptr;
    }
    thisClass->taskCount--;
    Base::TryCloseHandle((uv_handle_t *)handle, Base::CloseTimerCallback);
}

// remove step1
void HdcSessionBase::BeginRemoveTask(HTaskInfo hTask)
{
    StartTraceScope("HdcSessionBase::BeginRemoveTask");
    if (hTask == nullptr) {
        WRITE_LOG(LOG_FATAL, "BeginRemoveTask hTask is null");
        return;
    }
    if (hTask->taskStop || hTask->taskFree) {
        WRITE_LOG(LOG_WARN, "BeginRemoveTask channelId:%u taskStop:%d taskFree:%d",
            hTask->channelId, hTask->taskStop, hTask->taskFree);
        return;
    }

    WRITE_LOG(LOG_WARN, "BeginRemoveTask taskType:%d channelId:%u", hTask->taskType, hTask->channelId);
    Base::TimerUvTask(hTask->runLoop, hTask, TaskClassDeleteRetry,
                      (GLOBAL_TIMEOUT * TIME_BASE) / UV_DEFAULT_INTERVAL);

    hTask->taskStop = true;
}

// Clear all Task or a single Task, the regular situation is stopped first, and the specific class memory is cleaned up
// after the end of the LOOP.
// When ChannelIdinput == 0, at this time, all of the LOOP ends, all runs in the class end, so directly skip STOP,
// physical memory deletion class trimming
void HdcSessionBase::ClearOwnTasks(HSession hSession, const uint32_t channelIDInput)
{
    // First case: normal task cleanup process (STOP Remove)
    // Second: The task is cleaned up, the session ends
    // Third: The task is cleaned up, and the session is directly over the session.
    StartTraceScope("HdcSessionBase::ClearOwnTasks");
    hSession->mapTaskMutex.lock();
    map<uint32_t, HTaskInfo>::iterator iter;
    taskCount = hSession->mapTask->size();
    for (iter = hSession->mapTask->begin(); iter != hSession->mapTask->end();) {
        uint32_t channelId = iter->first;
        HTaskInfo hTask = iter->second;
        if (channelIDInput != 0) {  // single
            if (channelIDInput != channelId) {
                ++iter;
                continue;
            }
            BeginRemoveTask(hTask);
            WRITE_LOG(LOG_WARN, "ClearOwnTasks OP_CLEAR finish, sessionId:%s channelIDInput:%u",
                Hdc::MaskSessionIdToString(hSession->sessionId).c_str(), channelIDInput);
            iter = hSession->mapTask->erase(iter);
            break;
        }
        // multi
        BeginRemoveTask(hTask);
        iter = hSession->mapTask->erase(iter);
    }
    hSession->mapTaskMutex.unlock();
}

void HdcSessionBase::ClearSessions()
{
    // no need to lock mapSession
    // broadcast free signal
    for (auto v : mapSession) {
        HSession hSession = (HSession)v.second;
        if (!hSession->isDead) {
            FreeSession(hSession->sessionId);
        }
    }
}

void HdcSessionBase::ReMainLoopForInstanceClear()
{  // reloop
    StartTraceScope("HdcSessionBase::ReMainLoopForInstanceClear");
    auto clearSessionsForFinish = [](uv_idle_t *handle) -> void {
        HdcSessionBase *thisClass = (HdcSessionBase *)handle->data;
        if (thisClass->sessionRef > 0) {
            return;
        }
        // all task has been free
        uv_close((uv_handle_t *)handle, Base::CloseIdleCallback);
        uv_stop(&thisClass->loopMain);
    };
    Base::IdleUvTask(&loopMain, this, clearSessionsForFinish);
    uv_run(&loopMain, UV_RUN_DEFAULT);
};

#ifdef HDC_SUPPORT_UART
void HdcSessionBase::EnumUARTDeviceRegister(UartKickoutZombie kickOut)
{
    uv_rwlock_rdlock(&lockMapSession);
    map<uint32_t, HSession>::iterator i;
    for (i = mapSession.begin(); i != mapSession.end(); ++i) {
        HSession hs = i->second;
        if ((hs->connType != CONN_SERIAL) or (hs->hUART == nullptr)) {
            continue;
        }
        kickOut(hs);
        break;
    }
    uv_rwlock_rdunlock(&lockMapSession);
}
#endif

void HdcSessionBase::EnumUSBDeviceRegister(void (*pCallBack)(HSession hSession))
{
    if (!pCallBack) {
        return;
    }
    uv_rwlock_rdlock(&lockMapSession);
    map<uint32_t, HSession>::iterator i;
    for (i = mapSession.begin(); i != mapSession.end(); ++i) {
        HSession hs = i->second;
        if (hs->connType != CONN_USB) {
            continue;
        }
        if (hs->hUSB == nullptr) {
            continue;
        }
        if (pCallBack) {
            pCallBack(hs);
        }
        break;
    }
    uv_rwlock_rdunlock(&lockMapSession);
}

// The PC side gives the device information, determines if the USB device is registered
// PDEV and Busid Devid two choices
HSession HdcSessionBase::QueryUSBDeviceRegister(void *pDev, uint8_t busIDIn, uint8_t devIDIn)
{
#ifdef HDC_HOST
    libusb_device *dev = (libusb_device *)pDev;
    HSession hResult = nullptr;
    if (!mapSession.size()) {
        return nullptr;
    }
    uint8_t busId = 0;
    uint8_t devId = 0;
    if (pDev) {
        busId = libusb_get_bus_number(dev);
        devId = libusb_get_device_address(dev);
    } else {
        busId = busIDIn;
        devId = devIDIn;
    }
    uv_rwlock_rdlock(&lockMapSession);
    map<uint32_t, HSession>::iterator i;
    for (i = mapSession.begin(); i != mapSession.end(); ++i) {
        HSession hs = i->second;
        if (hs->connType == CONN_USB) {
            continue;
        }
        if (hs->hUSB == nullptr) {
            continue;
        }
        if (hs->hUSB->devId != devId || hs->hUSB->busId != busId) {
            continue;
        }
        hResult = hs;
        break;
    }
    uv_rwlock_rdunlock(&lockMapSession);
    return hResult;
#else
    return nullptr;
#endif
}

void HdcSessionBase::AsyncMainLoopTask(uv_idle_t *handle)
{
    AsyncParam *param = (AsyncParam *)handle->data;
    HdcSessionBase *thisClass = (HdcSessionBase *)param->thisClass;
    CALLSTAT_GUARD(thisClass->loopMainStatus, handle->loop, "HdcSessionBase::AsyncMainLoopTask");
    switch (param->method) {
        case ASYNC_FREE_SESSION:
            // Destruction is unified in the main thread
            thisClass->FreeSession(param->sid);
            break;
        case ASYNC_STOP_MAINLOOP:
            uv_stop(&thisClass->loopMain);
            break;
        default:
            break;
    }
    if (param->data) {
        delete[]((uint8_t *)param->data);
    }
    delete param;
    param = nullptr;
    Base::TryCloseHandle((uv_handle_t *)handle, Base::CloseIdleCallback);
}

void HdcSessionBase::MainAsyncCallback(uv_async_t *handle)
{
    HdcSessionBase *thisClass = (HdcSessionBase *)handle->data;
    CALLSTAT_GUARD(thisClass->loopMainStatus, handle->loop, "HdcSessionBase::MainAsyncCallback");
    list<void *>::iterator i;
    list<void *> &lst = thisClass->lstMainThreadOP;
    uv_rwlock_wrlock(&thisClass->mainAsync);
    for (i = lst.begin(); i != lst.end();) {
        AsyncParam *param = (AsyncParam *)*i;
        Base::IdleUvTask(&thisClass->loopMain, param, AsyncMainLoopTask);
        i = lst.erase(i);
    }
    uv_rwlock_wrunlock(&thisClass->mainAsync);
}

void HdcSessionBase::PushAsyncMessage(const uint32_t sessionId, const uint8_t method, const void *data,
                                      const int dataSize)
{
    AsyncParam *param = new AsyncParam();
    if (!param) {
        return;
    }
    param->sid = sessionId;
    param->thisClass = this;
    param->method = method;
    if (dataSize > 0) {
        param->dataSize = dataSize;
        param->data = new uint8_t[param->dataSize]();
        if (!param->data) {
            delete param;
            return;
        }
        if (memcpy_s((uint8_t *)param->data, param->dataSize, data, dataSize)) {
            delete[]((uint8_t *)param->data);
            delete param;
            return;
        }
    }

    asyncMainLoop.data = this;
    uv_rwlock_wrlock(&mainAsync);
    lstMainThreadOP.push_back(param);
    uv_rwlock_wrunlock(&mainAsync);
    uv_async_send(&asyncMainLoop);
}

void HdcSessionBase::WorkerPendding()
{
    StartLoopMonitor();
    uv_run(&loopMain, UV_RUN_DEFAULT);
    ClearInstanceResource();
}

int HdcSessionBase::MallocSessionByConnectType(HSession hSession)
{
    int ret = 0;
    switch (hSession->connType) {
        case CONN_TCP: {
            uv_tcp_init(&loopMain, &hSession->hWorkTCP);
            WRITE_LOG(LOG_INFO, "MallocSessionByConnectType init hWorkTCP sid:%s",
                Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
            ++hSession->uvHandleRef;
            hSession->hWorkTCP.data = hSession;
            HdcStatisticReporter::GetInstance().IncrCommandInfo(STATISTIC_ITEM::TCONN_COUNT);
            break;
        }
        case CONN_USB: {
            // Some members need to be placed at the primary thread
            HUSB hUSB = new HdcUSB();
            if (!hUSB) {
                ret = -1;
                break;
            }
            hSession->hUSB = hUSB;
            hSession->hUSB->wMaxPacketSizeSend = MAX_PACKET_SIZE_HISPEED;
            break;
        }
#ifdef HDC_SUPPORT_UART
        case CONN_SERIAL: {
            HUART hUART = new HdcUART();
            if (!hUART) {
                ret = -1;
                break;
            }
            hSession->hUART = hUART;
            break;
        }
#endif // HDC_SUPPORT_UART
        default:
            ret = -1;
            break;
    }
    return ret;
}

// Avoid unit test when client\server\daemon on the same host, maybe get the same ID value
uint32_t HdcSessionBase::GetSessionPseudoUid()
{
    uint32_t uid = 0;
    do {
        uid = Base::GetSecureRandom();
    } while (AdminSession(OP_QUERY, uid, nullptr) != nullptr);
    return uid;
}

// when client 0 to automatic generated，when daemon First place 1 followed by
HSession HdcSessionBase::MallocSession(bool serverOrDaemon, const ConnType connType, void *classModule,
                                       uint32_t sessionId)
{
#ifdef CONFIG_USE_JEMALLOC_DFX_INIF
    mallopt(M_DELAYED_FREE, M_DELAYED_FREE_DISABLE);
    mallopt(M_SET_THREAD_CACHE, M_THREAD_CACHE_DISABLE);
#endif
    HSession hSession = new(std::nothrow) HdcSession();
    if (!hSession) {
        WRITE_LOG(LOG_FATAL, "MallocSession new hSession failed");
        return nullptr;
    }
    int ret = 0;
    ++sessionRef;
    hSession->classInstance = this;
    hSession->connType = connType;
    hSession->classModule = classModule;
    hSession->isDead = false;
    hSession->sessionId = ((sessionId == 0) ? GetSessionPseudoUid() : sessionId);
    hSession->serverOrDaemon = serverOrDaemon;
    hSession->hWorkThread = uv_thread_self();
    hSession->mapTask = new(std::nothrow) map<uint32_t, HTaskInfo>();
    if (hSession->mapTask == nullptr) {
        WRITE_LOG(LOG_FATAL, "MallocSession new hSession->mapTask failed");
        delete hSession;
        hSession = nullptr;
        return nullptr;
    }
    hSession->listKey = new(std::nothrow) list<void *>;
    if (hSession->listKey == nullptr) {
        WRITE_LOG(LOG_FATAL, "MallocSession new hSession->listKey failed");
        delete hSession;
        hSession = nullptr;
        return nullptr;
    }
    uv_loop_init(&hSession->childLoop);
    hSession->childLoopStatus.StartReportTimer();
    hSession->uvHandleRef = 0;
    // pullup child
    WRITE_LOG(LOG_INFO, "HdcSessionBase NewSession, sessionId:%s, connType:%d.",
              Hdc::MaskSessionIdToString(hSession->sessionId).c_str(), hSession->connType);
    ++hSession->uvHandleRef;
    Base::CreateSocketPair(hSession->ctrlFd);
    size_t handleSize = sizeof(uv_poll_t);
    hSession->pollHandle[STREAM_WORK] = (uv_poll_t *)malloc(handleSize);
    hSession->pollHandle[STREAM_MAIN] = (uv_poll_t *)malloc(handleSize);
    uv_poll_t *pollHandleMain = hSession->pollHandle[STREAM_MAIN];
    if (pollHandleMain == nullptr || hSession->pollHandle[STREAM_WORK] == nullptr) {
        WRITE_LOG(LOG_FATAL, "MallocSession malloc hSession->pollHandle failed");
        delete hSession;
        hSession = nullptr;
        return nullptr;
    }
    (void)memset_s(hSession->pollHandle[STREAM_WORK], handleSize, 0, handleSize);
    (void)memset_s(hSession->pollHandle[STREAM_MAIN], handleSize, 0, handleSize);
    int initResult = uv_poll_init_socket(&loopMain, pollHandleMain, hSession->ctrlFd[STREAM_MAIN]);
    if (initResult != 0) {
        WRITE_LOG(LOG_FATAL, "MallocSession init pollHandleMain->loop failed");
        _exit(0);
    }
    uv_poll_start(pollHandleMain, UV_READABLE, ReadCtrlFromSession);
    hSession->pollHandle[STREAM_MAIN]->data = hSession;
    hSession->pollHandle[STREAM_WORK]->data = hSession;
    // Activate USB DAEMON's data channel, may not for use
    uv_tcp_init(&loopMain, &hSession->dataPipe[STREAM_MAIN]);
    (void)memset_s(&hSession->dataPipe[STREAM_WORK], sizeof(hSession->dataPipe[STREAM_WORK]),
                   0, sizeof(uv_tcp_t));
    ++hSession->uvHandleRef;
    int createResult = Base::CreateSocketPair(hSession->dataFd);
    if (createResult < 0) {
        WRITE_LOG(LOG_FATAL, "MallocSession init dataFd failed");
        _exit(0);
    }
    uv_tcp_open(&hSession->dataPipe[STREAM_MAIN], hSession->dataFd[STREAM_MAIN]);
    hSession->dataPipe[STREAM_MAIN].data = hSession;
    hSession->dataPipe[STREAM_WORK].data = hSession;
#ifdef HDC_HOST
    Base::SetTcpOptions(&hSession->dataPipe[STREAM_MAIN], HOST_SOCKETPAIR_SIZE);
#else
    Base::SetTcpOptions(&hSession->dataPipe[STREAM_MAIN]);
#endif
    ret = MallocSessionByConnectType(hSession);
    if (ret) {
        delete hSession;
        hSession = nullptr;
    } else {
        AdminSession(OP_ADD, hSession->sessionId, hSession);
    }
    return hSession;
}

#ifdef HDC_HOST
void HdcSessionBase::PrintAllSessionConnection(const uint32_t sessionId)
{
    AdminSession(OP_PRINT, sessionId, nullptr);
}
#endif

void HdcSessionBase::FreeSessionByConnectType(HSession hSession)
{
    WRITE_LOG(LOG_INFO, "FreeSessionByConnectType %s", hSession->ToDebugString().c_str());

    if (hSession->connType == CONN_USB) {
        // ibusb All context is applied for sub-threaded, so it needs to be destroyed in the subline
        if (!hSession->hUSB) {
            return;
        }
        HUSB hUSB = hSession->hUSB;
        if (!hUSB) {
            return;
        }
#ifdef HDC_HOST
        if (hUSB->devHandle) {
            libusb_release_interface(hUSB->devHandle, hUSB->interfaceNumber);
            libusb_close(hUSB->devHandle);
            hUSB->devHandle = nullptr;
        }
#else
        Base::CloseFd(hUSB->bulkIn);
        Base::CloseFd(hUSB->bulkOut);
#endif
        delete hSession->hUSB;
        hSession->hUSB = nullptr;
    }
#ifdef HDC_SUPPORT_UART
    if (CONN_SERIAL == hSession->connType) {
        if (!hSession->hUART) {
            return;
        }
        HUART hUART = hSession->hUART;
        if (!hUART) {
            return;
        }
        HdcUARTBase *uartBase = (HdcUARTBase *)hSession->classModule;
        // tell uart session will be free
        uartBase->StopSession(hSession);
#ifdef HDC_HOST
#ifdef HOST_MINGW
        if (hUART->devUartHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(hUART->devUartHandle);
            hUART->devUartHandle = INVALID_HANDLE_VALUE;
        }
#elif defined(HOST_LINUX)
        Base::CloseFd(hUART->devUartHandle);
#endif // _WIN32
#endif
        delete hSession->hUART;
        hSession->hUART = nullptr;
    }
#endif
}

// work when libuv-handle at struct of HdcSession has all callback finished
void HdcSessionBase::FreeSessionFinally(uv_idle_t *handle)
{
    HSession hSession = (HSession)handle->data;
    HdcSessionBase *thisClass = (HdcSessionBase *)hSession->classInstance;
    if (hSession->uvHandleRef > 0) {
        WRITE_LOG(LOG_INFO, "FreeSessionFinally uvHandleRef:%d sessionId:%s",
            hSession->uvHandleRef, Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        return;
    }
    // Notify Server or Daemon, just UI or display commandline
    thisClass->NotifyInstanceSessionFree(hSession, true);
#ifdef HDC_HOST
    thisClass->AdminSession(OP_PRINT, hSession->sessionId, nullptr);
#endif
#ifdef HDC_SUPPORT_ENCRYPT_TCP
    if (hSession->classSSL != nullptr) {
        HdcSSLBase *hssl = static_cast<HdcSSLBase *>(hSession->classSSL);
        delete hssl;
        hSession->classSSL = nullptr;
    }
#endif
    // all hsession uv handle has been clear
    thisClass->AdminSession(OP_REMOVE, hSession->sessionId, nullptr);
    WRITE_LOG(LOG_INFO, "!!!FreeSessionFinally sessionId:%s finish",
        Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
    HdcAuth::FreeKey(!hSession->serverOrDaemon, hSession->listKey);
    delete hSession;
    hSession = nullptr;  // fix CodeMars SetNullAfterFree issue
    Base::TryCloseHandle((const uv_handle_t *)handle, Base::CloseIdleCallback);
    --thisClass->sessionRef;
}

// work when child-work thread finish
void HdcSessionBase::FreeSessionContinue(HSession hSession)
{
    auto closeSessionTCPHandle = [](uv_handle_t *handle) -> void {
        HSession hSession = (HSession)handle->data;
        --hSession->uvHandleRef;
        Base::TryCloseHandle((uv_handle_t *)handle);
        if (handle == reinterpret_cast<uv_handle_t *>(hSession->pollHandle[STREAM_MAIN])) {
            WRITE_LOG(LOG_INFO, "closeSessionTCPHandle CloseSocketPair ctrlFd, sid:%s",
                Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
            Base::CloseSocketPair(hSession->ctrlFd);
            free(hSession->pollHandle[STREAM_MAIN]);
        }
    };
    if (hSession->connType == CONN_TCP) {
        // Turn off TCP to prevent continuing writing
        WRITE_LOG(LOG_INFO, "FreeSessionContinue start close hWorkTCP, sid:%s",
            Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        Base::TryCloseHandle((uv_handle_t *)&hSession->hWorkTCP, true, closeSessionTCPHandle);
#ifdef _WIN32
        closesocket(hSession->dataFd[STREAM_WORK]);
#else
        Base::CloseFd(hSession->dataFd[STREAM_WORK]);
#endif
    }
    hSession->availTailIndex = 0;
    if (hSession->ioBuf) {
        delete[] hSession->ioBuf;
        hSession->ioBuf = nullptr;
    }
    Base::TryCloseHandle((uv_handle_t *)hSession->pollHandle[STREAM_MAIN], true, closeSessionTCPHandle);
    Base::TryCloseHandle((uv_handle_t *)&hSession->dataPipe[STREAM_MAIN], true, closeSessionTCPHandle);
    FreeSessionByConnectType(hSession);
    // finish
    Base::IdleUvTask(&loopMain, hSession, FreeSessionFinally);
    HdcStatisticReporter& reporter = HdcStatisticReporter::GetInstance();
    uint64_t freeTime = Base::GetRuntimeMSec() - freeBegin;
    reporter.UpdateFreeSessionMaxCost(freeTime);
    if (freeTime > MAX_FREEZE_TIME) {
        reporter.IncrCommandInfo(STATISTIC_ITEM::FREEZE_COUNT);
    }
}

void HdcSessionBase::FreeSessionOpeate(uv_timer_t *handle)
{
    StartTraceScope("HdcSessionBase::FreeSessionOpeate");
    HSession hSession = (HSession)handle->data;
#ifdef HDC_HOST
    if (hSession->hUSB != nullptr
        && (!hSession->hUSB->hostBulkIn.isShutdown || !hSession->hUSB->hostBulkOut.isShutdown)) {
        HdcUSBBase *pUSB = ((HdcUSBBase *)hSession->classModule);
        pUSB->CancelUsbIo(hSession);
        return;
    }
#endif
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hSession->sessionId);
    HdcSessionBase *thisClass = (HdcSessionBase *)hSession->classInstance;
    if (hSession->ref > 0) {
        WRITE_LOG(LOG_WARN, "FreeSessionOpeate sid:%s ref:%u > 0", sessionIdMaskStr.c_str(), uint32_t(hSession->ref));
        return;
    }
    WRITE_LOG(LOG_INFO, "FreeSessionOpeate sid:%s ref:%u", sessionIdMaskStr.c_str(), uint32_t(hSession->ref));
    // wait workthread to free
    if (hSession->pollHandle[STREAM_WORK]->loop) {
        auto ctrl = BuildCtrlString(SP_STOP_SESSION, 0, nullptr, 0);
        Base::SendToPollFd(hSession->ctrlFd[STREAM_MAIN], ctrl.data(), ctrl.size());
        WRITE_LOG(LOG_INFO, "FreeSessionOpeate, send workthread for free. sessionId:%s", sessionIdMaskStr.c_str());
        auto callbackCheckFreeSessionContinue = [](uv_timer_t *handle) -> void {
            HSession hSession = (HSession)handle->data;
            HdcSessionBase *thisClass = (HdcSessionBase *)hSession->classInstance;
            if (!hSession->childCleared) {
                WRITE_LOG(LOG_INFO, "FreeSessionOpeate childCleared:%d sessionId:%s",
                    hSession->childCleared, Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
                return;
            }
            Base::TryCloseHandle((uv_handle_t *)handle, Base::CloseTimerCallback);
            thisClass->FreeSessionContinue(hSession);
        };
        Base::TimerUvTask(&thisClass->loopMain, hSession, callbackCheckFreeSessionContinue);
    } else {
        thisClass->FreeSessionContinue(hSession);
    }
    Base::TryCloseHandle((uv_handle_t *)handle, Base::CloseTimerCallback);
}

void HdcSessionBase::FreeSession(const uint32_t sessionId)
{
    StartTraceScope("HdcSessionBase::FreeSession");
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(sessionId);
    DispAllLoopStatus("FreeSession-" + sessionIdMaskStr);
    Base::AddDeletedSessionId(sessionId);
    if (threadSessionMain != uv_thread_self()) {
        PushAsyncMessage(sessionId, ASYNC_FREE_SESSION, nullptr, 0);
        return;
    }
    HSession hSession = AdminSession(OP_QUERY, sessionId, nullptr);
    WRITE_LOG(LOG_INFO, "Begin to free session, sessionid:%s", sessionIdMaskStr.c_str());
    freeBegin = Base::GetRuntimeMSec();
    do {
        if (!hSession || hSession->isDead) {
            WRITE_LOG(LOG_WARN, "FreeSession hSession nullptr or isDead sessionId:%s", sessionIdMaskStr.c_str());
            break;
        }
        WRITE_LOG(LOG_INFO, "dataFdSend:%llu, dataFdRecv:%llu",
            uint64_t(hSession->stat.dataSendBytes),
            uint64_t(hSession->stat.dataRecvBytes));
        hSession->isDead = true;
        Base::TimerUvTask(&loopMain, hSession, FreeSessionOpeate);
        NotifyInstanceSessionFree(hSession, false);
        WRITE_LOG(LOG_INFO, "FreeSession sessionId:%s ref:%u", sessionIdMaskStr.c_str(), uint32_t(hSession->ref));
    } while (false);
}

#ifdef HDC_HOST
void HdcSessionBase::PrintSession(const uint32_t sessionId)
{
    uv_rwlock_rdlock(&lockMapSession);
    int count = 0;
    for (auto v : mapSession) {
        HSession hSession = (HSession)v.second;
        auto str = hSession->ToDisplayConnectionStr();
        if (hSession->sessionId == sessionId) {
            str = str + " (Current)";
            WRITE_LOG(LOG_INFO, "%s", str.c_str());
        } else {
            WRITE_LOG(LOG_DEBUG, "%s", str.c_str());
        }
        if (hSession->isRunningOk) {
            count++;
        }
    }
    WRITE_LOG(LOG_INFO, "alive session count:%d", count);
    uv_rwlock_rdunlock(&lockMapSession);
}
#endif

HSession HdcSessionBase::VoteReset(const uint32_t sessionId)
{
    HSession hRet = nullptr;
    bool needReset = true;
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(sessionId);
    if (serverOrDaemon) {
        uv_rwlock_wrlock(&lockMapSession);
        hRet = mapSession[sessionId];
        hRet->voteReset = true;
        for (auto &kv : mapSession) {
            if (sessionId == kv.first) {
                continue;
            }
            WRITE_LOG(LOG_DEBUG, "session:%s vote reset, session %s is %s", sessionIdMaskStr.c_str(),
                Hdc::MaskSessionIdToString(kv.first).c_str(), kv.second->voteReset ? "YES" : "NO");
            if (!kv.second->voteReset) {
                needReset = false;
            }
        }
        uv_rwlock_wrunlock(&lockMapSession);
    }
    if (needReset) {
        WRITE_LOG(LOG_FATAL, "!! session:%s vote reset, passed unanimously !!", sessionIdMaskStr.c_str());
        abort();
    }
    return hRet;
}

HSession HdcSessionBase::AdminSession(const uint8_t op, const uint32_t sessionId, HSession hInput)
{
    StartTraceScope("HdcSessionBase::AdminSession");
    HSession hRet = nullptr;
    switch (op) {
        case OP_ADD:
            uv_rwlock_wrlock(&lockMapSession);
            mapSession[sessionId] = hInput;
            uv_rwlock_wrunlock(&lockMapSession);
            break;
        case OP_REMOVE:
            uv_rwlock_wrlock(&lockMapSession);
            mapSession.erase(sessionId);
            uv_rwlock_wrunlock(&lockMapSession);
            break;
        case OP_QUERY:
            uv_rwlock_rdlock(&lockMapSession);
            if (mapSession.count(sessionId)) {
                hRet = mapSession[sessionId];
            }
            uv_rwlock_rdunlock(&lockMapSession);
            break;
        case OP_PRINT:
#ifdef HDC_HOST
            PrintSession(sessionId);
#endif
            break;
        case OP_QUERY_REF:
            uv_rwlock_wrlock(&lockMapSession);
            if (mapSession.count(sessionId)) {
                hRet = mapSession[sessionId];
                ++hRet->ref;
            }
            uv_rwlock_wrunlock(&lockMapSession);
            break;
        case OP_UPDATE:
            uv_rwlock_wrlock(&lockMapSession);
            // remove old
            mapSession.erase(sessionId);
            mapSession[hInput->sessionId] = hInput;
            uv_rwlock_wrunlock(&lockMapSession);
            break;
        case OP_VOTE_RESET:
            if (mapSession.count(sessionId) == 0) {
                break;
            }
            hRet = VoteReset(sessionId);
            break;
        default:
            break;
    }
    return hRet;
}

#ifndef HDC_HOST
void HdcSessionBase::AdminUsbSession()
{
    StartTraceScope("HdcSessionBase::AdminUsbSession");
    HSession hSession = nullptr;
    uv_rwlock_wrlock(&lockMapSession);
    for (const auto& [_, hRet] : mapSession) {
        if (hRet->connType == CONN_USB) {
            hSession = hRet;
            break;
        }
    }
    uv_rwlock_wrunlock(&lockMapSession);

    if (hSession != nullptr) {
        FreeSession(hSession->sessionId);
    }
}
#endif

void HdcSessionBase::DumpTasksInfo(map<uint32_t, HTaskInfo> &mapTask)
{
    int idx = 1;
    for (auto t : mapTask) {
        HTaskInfo ti = t.second;
        WRITE_LOG(LOG_WARN, "%d: channelId: %lu, type: %d, closeRetry: %d\n",
                  idx++, ti->channelId, ti->taskType, ti->closeRetryCount);
    }
}

// All in the corresponding sub-thread, no need locks
HTaskInfo HdcSessionBase::AdminTask(const uint8_t op, HSession hSession, const uint32_t channelId, HTaskInfo hInput)
{
    HTaskInfo hRet = nullptr;
    map<uint32_t, HTaskInfo> &mapTask = *hSession->mapTask;

    switch (op) {
        case OP_ADD:
            hRet = mapTask[channelId];
            if (hRet != nullptr) {
                delete hRet;
            }
            mapTask[channelId] = hInput;
            hRet = hInput;

            WRITE_LOG(LOG_INFO, "AdminTask add task type:%u cid:%u sid:%s mapsize:%zu",
                      hInput->taskType, channelId,
                      Hdc::MaskSessionIdToString(hSession->sessionId).c_str(), mapTask.size());

            break;
        case OP_REMOVE:
            mapTask.erase(channelId);
            WRITE_LOG(LOG_INFO, "AdminTask rm cid:%u sid:%s mapsize:%zu",
                      channelId, Hdc::MaskSessionIdToString(hSession->sessionId).c_str(), mapTask.size());
            break;
        case OP_QUERY:
            if (mapTask.count(channelId)) {
                hRet = mapTask[channelId];
            }
            break;
        case OP_VOTE_RESET:
            AdminSession(op, hSession->sessionId, nullptr);
            break;
        default:
            break;
    }
    return hRet;
}

static void PrintDefaultInfo(HSession hSession)
{
    static std::atomic<int> printCount(0);
    if (printCount.load() <= 0) {
        WRITE_LOG(LOG_WARN, "SendByProtocol enter default, type:%u, sid:%s",
            hSession->connType, Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        printCount.store(100); // 100 means print once for every 100 calls
    }
    printCount.fetch_sub(1);
}

int HdcSessionBase::SendByProtocol(HSession hSession, uint8_t *bufPtr, const int bufLen, bool echo)
{
    StartTraceScope("HdcSessionBase::SendByProtocol");
    if (hSession->isDead) {
        delete[] bufPtr;
        WRITE_LOG(LOG_WARN, "SendByProtocol session dead error");
        return ERR_SESSION_NOFOUND;
    }
    int ret = 0;
    switch (hSession->connType) {
        case CONN_TCP: {
            HdcTCPBase *pTCP = ((HdcTCPBase *)hSession->classModule);
            if (echo && !hSession->serverOrDaemon) {
                ret = pTCP->WriteUvTcpFd(&hSession->hChildWorkTCP, bufPtr, bufLen);
            } else {
                if (hSession->hWorkThread == uv_thread_self()) {
                    ret = pTCP->WriteUvTcpFd(&hSession->hWorkTCP, bufPtr, bufLen);
                } else {
                    ret = pTCP->WriteUvTcpFd(&hSession->hChildWorkTCP, bufPtr, bufLen);
                }
            }
            break;
        }
        case CONN_USB: {
            HdcUSBBase *pUSB = ((HdcUSBBase *)hSession->classModule);
            ret = pUSB->SendUSBBlock(hSession, bufPtr, bufLen);
            delete[] bufPtr;
            break;
        }
#ifdef HDC_SUPPORT_UART
        case CONN_SERIAL: {
            HdcUARTBase *pUART = ((HdcUARTBase *)hSession->classModule);
            ret = pUART->SendUARTData(hSession, bufPtr, bufLen);
            delete[] bufPtr;
            break;
        }
#endif
        default:
            PrintDefaultInfo(hSession);
            delete[] bufPtr;
            break;
    }
    return ret;
}

int HdcSessionBase::Send(const uint32_t sessionId, const uint32_t channelId, const uint16_t commandFlag,
                         const uint8_t *data, const int dataSize)
{
    StartTraceScope("HdcSessionBase::Send");
    HSession hSession = AdminSession(OP_QUERY_REF, sessionId, nullptr);
    if (!hSession) {
        WRITE_LOG(LOG_WARN, "Send to offline device, drop it, sessionId:%s",
            Hdc::MaskSessionIdToString(sessionId).c_str());
        return ERR_SESSION_NOFOUND;
    }
    PayloadProtect protectBuf;  // noneed convert to big-endian
    protectBuf.channelId = channelId;
    protectBuf.commandFlag = commandFlag;
    protectBuf.checkSum = (ENABLE_IO_CHECKSUM && dataSize > 0) ? Base::CalcCheckSum(data, dataSize) : 0;
    protectBuf.vCode = payloadProtectStaticVcode;
    string s = SerialStruct::SerializeToString(protectBuf);
    // reserve for encrypt here
    // xx-encrypt

    PayloadHead payloadHead = {};  // need convert to big-endian
    payloadHead.flag[0] = PACKET_FLAG.at(0);
    payloadHead.flag[1] = PACKET_FLAG.at(1);
    payloadHead.protocolVer = VER_PROTOCOL;
    payloadHead.headSize = htons(s.size());
    payloadHead.dataSize = htonl(dataSize);
    size_t finalBufSize = sizeof(PayloadHead) + s.size() + dataSize;
#ifdef HDC_SUPPORT_ENCRYPT_TCP
    uint8_t *finayBuf = (hSession->connType == CONN_TCP && hSession->sslHandshake) ?
        new(std::nothrow) uint8_t[HdcSSLBase::GetSSLBufLen(finalBufSize) + 1]() :
        new(std::nothrow) uint8_t[finalBufSize]();
#else
    uint8_t *finayBuf = new(std::nothrow) uint8_t[finalBufSize]();
#endif
    if (finayBuf == nullptr) {
        WRITE_LOG(LOG_WARN, "send allocmem err");
        --hSession->ref;
        return ERR_BUF_ALLOC;
    }
    bool bufRet = false;
    do {
        if (memcpy_s(finayBuf, sizeof(PayloadHead), reinterpret_cast<uint8_t *>(&payloadHead), sizeof(PayloadHead))) {
            WRITE_LOG(LOG_WARN, "send copyhead err for dataSize:%d", dataSize);
            break;
        }
        if (memcpy_s(finayBuf + sizeof(PayloadHead), s.size(),
                     reinterpret_cast<uint8_t *>(const_cast<char *>(s.c_str())), s.size())) {
            WRITE_LOG(LOG_WARN, "send copyProtbuf err for dataSize:%d", dataSize);
            break;
        }
        if (dataSize > 0 && memcpy_s(finayBuf + sizeof(PayloadHead) + s.size(), dataSize, data, dataSize)) {
            WRITE_LOG(LOG_WARN, "send copyDatabuf err for dataSize:%d", dataSize);
            break;
        }
        bufRet = true;
    } while (false);
    if (!bufRet) {
        delete[] finayBuf;
        WRITE_LOG(LOG_WARN, "send copywholedata err for dataSize:%d", dataSize);
        --hSession->ref;
        return ERR_BUF_COPY;
    }
    int ret = -1;
    if (CMD_KERNEL_ECHO == commandFlag) {
        ret = SendByProtocol(hSession, finayBuf, finalBufSize, true);
    } else {
        ret = SendByProtocol(hSession, finayBuf, finalBufSize);
    }
    --hSession->ref;
    return ret;
}

int HdcSessionBase::DecryptPayload(HSession hSession, PayloadHead *payloadHeadBe, uint8_t *encBuf)
{
    StartTraceScope("HdcSessionBase::DecryptPayload");
    PayloadProtect protectBuf = {};
    uint16_t headSize = ntohs(payloadHeadBe->headSize);
    uint32_t dataSize = ntohl(payloadHeadBe->dataSize);
    string encString(reinterpret_cast<char *>(encBuf), headSize);
    SerialStruct::ParseFromString(protectBuf, encString);
    if (protectBuf.vCode != payloadProtectStaticVcode) {
        WRITE_LOG(LOG_FATAL, "Session recv static vcode failed");
        return ERR_BUF_CHECK;
    }
    uint8_t *data = encBuf + headSize;
    if (ENABLE_IO_CHECKSUM && protectBuf.checkSum != 0 && (protectBuf.checkSum != Base::CalcCheckSum(data, dataSize))) {
        WRITE_LOG(LOG_FATAL, "Session recv CalcCheckSum failed");
        return ERR_BUF_CHECK;
    }
    if (!FetchCommand(hSession, protectBuf.channelId, protectBuf.commandFlag, data, dataSize)) {
        WRITE_LOG(LOG_WARN, "FetchCommand failed: channelId %x commandFlag %x",
                  protectBuf.channelId, protectBuf.commandFlag);
        return ERR_GENERIC;
    }
    return RET_SUCCESS;
}

int HdcSessionBase::OnRead(HSession hSession, uint8_t *bufPtr, const int bufLen)
{
    int ret = ERR_GENERIC;
    StartTraceScope("HdcSessionBase::OnRead");
    if (memcmp(bufPtr, PACKET_FLAG.c_str(), PACKET_FLAG.size())) {
        WRITE_LOG(LOG_FATAL, "PACKET_FLAG incorrect %x %x", bufPtr[0], bufPtr[1]);
        return ERR_BUF_CHECK;
    }
    struct PayloadHead *payloadHead = reinterpret_cast<struct PayloadHead *>(bufPtr);
    // to prevent integer overflow caused by the add operation of two input num
    uint64_t payloadHeadSize = static_cast<uint64_t>(ntohl(payloadHead->dataSize)) +
        static_cast<uint64_t>(ntohs(payloadHead->headSize));
    int packetHeadSize = sizeof(struct PayloadHead);
    if (payloadHeadSize == 0 || payloadHeadSize > static_cast<uint64_t>(HDC_BUF_MAX_BYTES)) {
        WRITE_LOG(LOG_FATAL, "Packet size incorrect");
        return ERR_BUF_CHECK;
    }

    // 0 < payloadHeadSize < HDC_BUF_MAX_BYTES
    int tobeReadLen = static_cast<int>(payloadHeadSize);
    if (bufLen - packetHeadSize < tobeReadLen) {
        return 0;
    }
    if (DecryptPayload(hSession, payloadHead, bufPtr + packetHeadSize)) {
        WRITE_LOG(LOG_WARN, "decrypt plhead error");
        return ERR_BUF_CHECK;
    }
    ret = packetHeadSize + tobeReadLen;
    return ret;
}

// Returns <0 error;> 0 receives the number of bytes; 0 untreated
int HdcSessionBase::FetchIOBuf(HSession hSession, uint8_t *ioBuf, int read)
{
    HdcSessionBase *ptrConnect = (HdcSessionBase *)hSession->classInstance;
    int indexBuf = 0;
    int childRet = 0;
    StartTraceScope("HdcSessionBase::FetchIOBuf");
    if (read < 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        uv_strerror_r(read, buf, bufSize);
        WRITE_LOG(LOG_FATAL, "FetchIOBuf read io failed,%s", buf);
        return ERR_IO_FAIL;
    }
    if (hSession->heartbeat.GetSupportHeartbeat() && !hSession->heartbeat.HandleMessageCount()) {
        WRITE_LOG(LOG_FATAL, "Rehandshake is required because no heartbeat message is received for a long time");
        FreeSession(hSession->sessionId);
        return ERR_IO_FAIL;
    }
    hSession->stat.dataRecvBytes += read;
    hSession->availTailIndex += read;
    while (!hSession->isDead && hSession->availTailIndex > static_cast<int>(sizeof(PayloadHead))) {
        childRet = ptrConnect->OnRead(hSession, ioBuf + indexBuf, hSession->availTailIndex);
        if (childRet > 0) {
            hSession->availTailIndex -= childRet;
            indexBuf += childRet;
        } else if (childRet == 0) {
            // Not enough a IO
            break;
        } else {                           // <0
            WRITE_LOG(LOG_FATAL, "FetchIOBuf error childRet:%d sessionId:%s", childRet,
                Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
            hSession->availTailIndex = 0;  // Preventing malicious data packages
            indexBuf = ERR_BUF_SIZE;
            break;
        }
        // It may be multi-time IO to merge in a BUF, need to loop processing
    }
    if (indexBuf > 0 && hSession->availTailIndex > 0) {
        if (memmove_s(hSession->ioBuf, hSession->bufSize, hSession->ioBuf + indexBuf, hSession->availTailIndex)
            != EOK) {
            return ERR_BUF_COPY;
        };
        uint8_t *bufToZero = reinterpret_cast<uint8_t *>(hSession->ioBuf + hSession->availTailIndex);
        (void)memset_s(bufToZero, hSession->bufSize - hSession->availTailIndex,
            0, hSession->bufSize - hSession->availTailIndex);
    }
    return indexBuf;
}

#ifdef HDC_SUPPORT_ENCRYPT_TCP
int HdcSessionBase::FetchIOBuf(HSession hSession, uint8_t *ioBuf, int read, bool isEncrypt)
{
    if (isEncrypt) {
        HdcSSLBase *hssl = static_cast<HdcSSLBase *>(hSession->classSSL);
        int ret = (hssl != nullptr) ? hssl->Decrypt(read,
            hSession->bufSize, hSession->ioBuf, hSession->availTailIndex) : ERR_GENERIC;
        if (ret == ERR_DECRYPT_WANT_READ) {
            return RET_SUCCESS;
        }
        // for the SSL_ERR_WANT_READ branch, do next read.
        if (ret == RET_SUCCESS && FetchIOBuf(hSession, hSession->ioBuf, 0) < 0) {
            WRITE_LOG(LOG_FATAL, "ReadStream FetchIOBuf error nread:%zd, sid:%s", read,
                Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
            ret = ERR_GENERIC;
        }
        return ret;
    } else {
        return FetchIOBuf(hSession, hSession->ioBuf, read);
    }
}
#endif

void HdcSessionBase::AllocCallback(uv_handle_t *handle, size_t sizeWanted, uv_buf_t *buf)
{
    HSession context = (HSession)handle->data;
    Base::ReallocBuf(&context->ioBuf, &context->bufSize, HDC_SOCKETPAIR_SIZE);
    buf->base = (char *)context->ioBuf + context->availTailIndex;
    int size = context->bufSize - context->availTailIndex;
    buf->len = std::min(size, static_cast<int>(sizeWanted));
}

void HdcSessionBase::FinishWriteSessionTCP(uv_write_t *req, int status)
{
    HSession hSession = (HSession)req->handle->data;
    --hSession->ref;
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hSession->sessionId);
    HdcSessionBase *thisClass = (HdcSessionBase *)hSession->classInstance;
    if (status < 0) {
        WRITE_LOG(LOG_WARN, "FinishWriteSessionTCP status:%d sessionId:%s isDead:%d ref:%u",
            status, sessionIdMaskStr.c_str(), hSession->isDead, uint32_t(hSession->ref));
        Base::TryCloseHandle((uv_handle_t *)req->handle);
        if (!hSession->isDead && !hSession->ref) {
            WRITE_LOG(LOG_INFO, "FinishWriteSessionTCP freesession:%s", sessionIdMaskStr.c_str());
            thisClass->FreeSession(hSession->sessionId);
        }
    }
    delete[]((uint8_t *)req->data);
    delete req;
}

bool HdcSessionBase::DispatchSessionThreadCommand(const uint8_t *baseBuf, const int bytesIO)
{
    bool ret = true;
    uint8_t flag = *const_cast<uint8_t *>(baseBuf);

    switch (flag) {
        case SP_JDWP_NEWFD:
        case SP_ARK_NEWFD: {
            JdwpNewFileDescriptor(baseBuf, bytesIO);
            break;
        }
        default:
            WRITE_LOG(LOG_WARN, "Not support session command");
            break;
    }
    return ret;
}

void HdcSessionBase::ReadCtrlFromSession(uv_poll_t *poll, int /* status */, int /* events */)
{
    HSession hSession = (HSession)poll->data;
    CALLSTAT_GUARD(hSession->childLoopStatus, poll->loop, "HdcSessionBase::ReadCtrlFromSession");
    HdcSessionBase *hSessionBase = (HdcSessionBase *)hSession->classInstance;
    const int size = Base::GetMaxBufSizeStable();
    char *buf = reinterpret_cast<char *>(new uint8_t[size]());
    ssize_t nread = Base::ReadFromFd(hSession->ctrlFd[STREAM_MAIN], buf, size);
    while (true) {
        if (nread < 0) {
            constexpr int bufSize = 1024;
            char buffer[bufSize] = { 0 };
            uv_strerror_r(static_cast<int>(nread), buffer, bufSize);
            WRITE_LOG(LOG_WARN, "ReadCtrlFromSession failed,%s", buffer);
            uv_poll_stop(poll);
            break;
        }
        if (nread == 0) {
            WRITE_LOG(LOG_FATAL, "ReadCtrlFromSession read data zero byte");
            break;
        }
        // only one command, no need to split command from stream
        // if add more commands, consider the format command
        hSessionBase->DispatchSessionThreadCommand(reinterpret_cast<uint8_t *>(buf), nread);
        break;
    }
    delete[] buf;
}

void HdcSessionBase::SetFeature(SessionHandShake &handshake, const uint8_t connType)
{
    Base::HdcFeatureSet feature = {
        Base::GetVersion(),
        conTypeDetail[connType >= CONN_UNKNOWN ? CONN_UNKNOWN : connType],
        GetOs(),
    };
    if (Base::GetheartbeatSwitch()) {
        feature.push_back(FEATURE_HEARTBEAT);
    }
#ifdef HDC_SUPPORT_ENCRYPT_TCP
    if (Base::GetEncrpytTCPSwitch()) {
        feature.push_back(FEATURE_ENCRYPT_TCP);
    }
#endif
#ifdef HOST_OHOS
    int connectValidationStatus = HdcValidation::GetConnectValidationParam();
    if (connectValidationStatus == VALIDATION_HDC_HOST || connectValidationStatus == VALIDATION_HDC_HOST_AND_DAEMON) {
        WRITE_LOG(LOG_INFO, "tttttttttttttttttttt HdcValidation::GetConnectValidationParam()");
        feature.push_back(FEATURE_CONN_VALIDATION);
    }
#endif
    // told daemon, we support features
    if (feature.size() == 0) {
        return;
    }
    Base::TlvAppend(handshake.buf, TAG_SUPPORT_FEATURE, Base::FeatureToString(feature));
}

void HdcSessionBase::WorkThreadInitSession(HSession hSession, SessionHandShake &handshake)
{
    handshake.banner = HANDSHAKE_MESSAGE;
    handshake.sessionId = hSession->sessionId;
    handshake.connectKey = hSession->connectKey;
    if (!hSession->isCheck) {
        handshake.version = Base::GetVersion() + HDC_MSG_HASH;
        WRITE_LOG(LOG_INFO, "set version = %s", handshake.version.c_str());
    }
    handshake.authType = AUTH_NONE;
    // told daemon, we support RSA_3072_SHA512 auth
    Base::TlvAppend(handshake.buf, TAG_AUTH_TYPE, std::to_string(AuthVerifyType::RSA_3072_SHA512));
    HdcSessionBase *hSessionBase = (HdcSessionBase *)hSession->classInstance;
    hSessionBase->SetFeature(handshake, hSession->connType);
}

bool HdcSessionBase::WorkThreadStartSession(HSession hSession)
{
    bool regOK = false;
    int childRet = 0;
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hSession->sessionId);
    if (hSession->connType == CONN_TCP) {
        HdcTCPBase *pTCPBase = (HdcTCPBase *)hSession->classModule;
        hSession->hChildWorkTCP.data = hSession;
        if (uv_tcp_init(&hSession->childLoop, &hSession->hChildWorkTCP) < 0) {
            WRITE_LOG(LOG_FATAL, "WorkThreadStartSession uv_tcp_init failed sid:%s", sessionIdMaskStr.c_str());
            return false;
        }
        WRITE_LOG(LOG_INFO, "start tcp open fdChildWorkTCP, sid:%s", sessionIdMaskStr.c_str());
        if ((childRet = uv_tcp_open(&hSession->hChildWorkTCP, hSession->fdChildWorkTCP)) < 0) {
            constexpr int bufSize = 1024;
            char buf[bufSize] = { 0 };
            uv_strerror_r(childRet, buf, bufSize);
            WRITE_LOG(LOG_FATAL, "WorkThreadStartSession uv_tcp_open failed fd:%d str:%s sid:%s",
                hSession->fdChildWorkTCP, buf, sessionIdMaskStr.c_str());
            return false;
        }
        Base::SetTcpOptions((uv_tcp_t *)&hSession->hChildWorkTCP);
        uv_read_start((uv_stream_t *)&hSession->hChildWorkTCP, AllocCallback, pTCPBase->ReadStream);
        regOK = true;
#ifdef HDC_SUPPORT_UART
    } else if (hSession->connType == CONN_SERIAL) { // UART
        HdcUARTBase *pUARTBase = (HdcUARTBase *)hSession->classModule;
        WRITE_LOG(LOG_INFO, "UART ReadyForWorkThread sid:%s", sessionIdMaskStr.c_str());
        regOK = pUARTBase->ReadyForWorkThread(hSession);
#endif
    } else {  // USB
        HdcUSBBase *pUSBBase = (HdcUSBBase *)hSession->classModule;
        WRITE_LOG(LOG_INFO, "USB ReadyForWorkThread sid:%s", sessionIdMaskStr.c_str());
        regOK = pUSBBase->ReadyForWorkThread(hSession);
    }

    if (regOK && hSession->serverOrDaemon) {
        // session handshake step1
        SessionHandShake handshake = {};
        WorkThreadInitSession(hSession, handshake);
        string hs = SerialStruct::SerializeToString(handshake);
#ifdef HDC_SUPPORT_UART
        WRITE_LOG(LOG_INFO, "WorkThreadStartSession session %s auth %u send handshake hs: %s",
            sessionIdMaskStr.c_str(), handshake.authType, hs.c_str());
#endif
        Send(hSession->sessionId, 0, CMD_KERNEL_HANDSHAKE,
             reinterpret_cast<uint8_t *>(const_cast<char *>(hs.c_str())), hs.size());
    }
    return regOK;
}

vector<uint8_t> HdcSessionBase::BuildCtrlString(InnerCtrlCommand command, uint32_t channelId, uint8_t *data,
                                                int dataSize)
{
    vector<uint8_t> ret;
    while (true) {
        if (dataSize > BUF_SIZE_MICRO) {
            WRITE_LOG(LOG_WARN, "BuildCtrlString dataSize:%d", dataSize);
            break;
        }
        CtrlStruct ctrl = {};
        ctrl.command = command;
        ctrl.channelId = channelId;
        ctrl.dataSize = dataSize;
        if (dataSize > 0 && data != nullptr && memcpy_s(ctrl.data, sizeof(ctrl.data), data, dataSize) != EOK) {
            break;
        }
        uint8_t *buf = reinterpret_cast<uint8_t *>(&ctrl);
        ret.insert(ret.end(), buf, buf + sizeof(CtrlStruct));
        break;
    }
    return ret;
}

bool HdcSessionBase::DispatchMainThreadCommand(HSession hSession, const CtrlStruct *ctrl)
{
    bool ret = true;
    uint32_t channelId = ctrl->channelId;  // if send not set, it is zero
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hSession->sessionId);
    switch (ctrl->command) {
        case SP_START_SESSION: {
            WRITE_LOG(LOG_WARN, "Dispatch MainThreadCommand  START_SESSION sessionId:%s instance:%s",
                sessionIdMaskStr.c_str(), hSession->serverOrDaemon ? "server" : "daemon");
            ret = WorkThreadStartSession(hSession);
            break;
        }
        case SP_STOP_SESSION: {
            WRITE_LOG(LOG_WARN, "Dispatch MainThreadCommand STOP_SESSION sessionId:%s", sessionIdMaskStr.c_str());
            HdcSessionBase *hSessionBase = (HdcSessionBase *)hSession->classInstance;
            hSessionBase->StopHeartbeatWork(hSession);
            auto closeSessionChildThreadTCPHandle = [](uv_handle_t *handle) -> void {
                HSession hSession = (HSession)handle->data;
                Base::TryCloseHandle((uv_handle_t *)handle);
                if (handle == (uv_handle_t *)hSession->pollHandle[STREAM_WORK]) {
                    free(hSession->pollHandle[STREAM_WORK]);
                }
                if (--hSession->uvChildRef == 0) {
                    WRITE_LOG(LOG_WARN, "Dispatch MainThreadCommand uv stop childLoop sessionId:%s",
                        Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
                    uv_stop(&hSession->childLoop);
                };
            };
            constexpr int uvChildRefOffset = 2;
            hSession->uvChildRef += uvChildRefOffset;
            if (hSession->connType == CONN_TCP && hSession->hChildWorkTCP.loop) {  // maybe not use it
                ++hSession->uvChildRef;
                WRITE_LOG(LOG_INFO, "Dispatch MainThreadCommand try close hChildWorkTCP, sid:%s",
                    sessionIdMaskStr.c_str());
                Base::TryCloseHandle((uv_handle_t *)&hSession->hChildWorkTCP, true, closeSessionChildThreadTCPHandle);
            }
            Base::TryCloseHandle((uv_handle_t *)hSession->pollHandle[STREAM_WORK], true,
                                 closeSessionChildThreadTCPHandle);
            Base::TryCloseHandle((uv_handle_t *)&hSession->dataPipe[STREAM_WORK], true,
                                 closeSessionChildThreadTCPHandle);
            break;
        }
        case SP_ATTACH_CHANNEL: {
            if (!serverOrDaemon) {
                break;  // Only Server has this feature
            }
            AttachChannel(hSession, channelId);
            break;
        }
        case SP_DEATCH_CHANNEL: {
            if (!serverOrDaemon) {
                break;  // Only Server has this feature
            }
            DetachChannel(hSession, channelId);
            break;
        }
        default:
            WRITE_LOG(LOG_WARN, "Not support main command");
            ret = false;
            break;
    }
    return ret;
}

// Several bytes of control instructions, generally do not stick
void HdcSessionBase::ReadCtrlFromMain(uv_poll_t *poll, int /* status */, int /* events */)
{
    HSession hSession = (HSession)poll->data;
    CALLSTAT_GUARD(hSession->childLoopStatus, poll->loop, "HdcSessionBase::ReadCtrlFromMain");
    HdcSessionBase *hSessionBase = (HdcSessionBase *)hSession->classInstance;
    int formatCommandSize = sizeof(CtrlStruct);
    int index = 0;
    const int size = Base::GetMaxBufSizeStable();
    char *buf = reinterpret_cast<char *>(new uint8_t[size]());
    ssize_t nread = Base::ReadFromFd(hSession->ctrlFd[STREAM_WORK], buf, size);
    while (true) {
        if (nread < 0) {
            constexpr int bufSize = 1024;
            char buffer[bufSize] = { 0 };
            uv_strerror_r(static_cast<int>(nread), buffer, bufSize);
            WRITE_LOG(LOG_WARN, "SessionCtrl failed,%s", buffer);
            break;
        }
        if (nread % formatCommandSize != 0) {
            WRITE_LOG(LOG_FATAL, "ReadCtrlFromMain size failed, nread == %d", nread);
            break;
        }
        CtrlStruct *ctrl = reinterpret_cast<CtrlStruct *>(buf + index);
        if (!hSessionBase->DispatchMainThreadCommand(hSession, ctrl)) {
            WRITE_LOG(LOG_FATAL, "ReadCtrlFromMain failed sessionId:%s channelId:%u command:%u",
                      Hdc::MaskSessionIdToString(hSession->sessionId).c_str(),
                      ctrl->channelId, ctrl->command);
            break;
        }
        index += sizeof(CtrlStruct);
        if (index >= nread) {
            break;
        }
    }
    delete[] buf;
}

void HdcSessionBase::ReChildLoopForSessionClear(HSession hSession)
{
    // Restart loop close task
    ClearOwnTasks(hSession, 0);
    WRITE_LOG(LOG_INFO, "ReChildLoopForSessionClear sessionId:%s",
        Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
    auto clearTaskForSessionFinish = [](uv_timer_t *handle) -> void {
        HSession hSession = (HSession)handle->data;
        hSession->clearTaskTimes++;
        HdcSessionBase *thisClass = (HdcSessionBase *)hSession->classInstance;
        if (thisClass->taskCount && (hSession->clearTaskTimes < 50)) {    //50: 50 * 120ms = 6s
            return;
        }
        if (hSession->clearTaskTimes >= 50) {   //50: 50 * 120ms = 6s
            WRITE_LOG(LOG_FATAL, "ReChildLoopForSessionClear exited due to timeout of 6s");
        }
        // all task has been free
        uv_close((uv_handle_t *)handle, Base::CloseTimerCallback);
        uv_stop(&hSession->childLoop);  // stop ReChildLoopForSessionClear pendding
    };
    Base::TimerUvTask(&hSession->childLoop, hSession, clearTaskForSessionFinish,
                      (GLOBAL_TIMEOUT * TIME_BASE) / UV_DEFAULT_INTERVAL);
    uv_run(&hSession->childLoop, UV_RUN_DEFAULT);
    // clear
    Base::TryCloseChildLoop(&hSession->childLoop, "Session childUV");
}

//stop Heartbeat
void HdcSessionBase::StopHeartbeatWork(HSession hSession)
{
    hSession->heartbeat.SetSupportHeartbeat(false);
    if (Base::GetheartbeatSwitch()) {
        WRITE_LOG(LOG_INFO, "session %s stop heartbeatTimer",
            Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        uv_timer_stop(&hSession->heartbeatTimer);
    }
}

// send heartbeat msg
void HdcSessionBase::SendHeartbeatMsg(uv_timer_t *handle)
{
    HSession hSession = (HSession)handle->data;
    if (!hSession->heartbeat.GetSupportHeartbeat()) {
        WRITE_LOG(LOG_INFO, "session %s not support heatbeat",
            Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        if (hSession->handshakeOK) {
            uv_timer_stop(&hSession->heartbeatTimer);
        }
        return;
    }

    if (!hSession->handshakeOK) {
        return;
    }

    std::string str = hSession->heartbeat.ToString();
    WRITE_LOG(LOG_INFO, "send %s for session %s", str.c_str(),
        Hdc::MaskSessionIdToString(hSession->sessionId).c_str());

    HeartbeatMsg heartbeat = {};
    heartbeat.heartbeatCount = hSession->heartbeat.GetHeartbeatCount();
    hSession->heartbeat.AddHeartbeatCount();
    string s = SerialStruct::SerializeToString(heartbeat);
    HdcSessionBase *thisClass = (HdcSessionBase *)hSession->classInstance;
    thisClass->Send(hSession->sessionId, 0, CMD_HEARTBEAT_MSG,
        reinterpret_cast<uint8_t *>(const_cast<char *>(s.c_str())), s.size());
}

void HdcSessionBase::StartHeartbeatWork(HSession hSession)
{
    if (!Base::GetheartbeatSwitch()) {
        return;
    }
    WRITE_LOG(LOG_DEBUG, "StartHeartbeatWork");
    hSession->heartbeatTimer.data = hSession;
    uv_timer_init(&hSession->childLoop, &hSession->heartbeatTimer);
    uv_timer_start(&hSession->heartbeatTimer, SendHeartbeatMsg, 0, HEARTBEAT_INTERVAL);
}

void HdcSessionBase::SessionWorkThread(uv_work_t *arg)
{
    HSession hSession = (HSession)arg->data;
    HdcSessionBase *thisClass = (HdcSessionBase *)hSession->classInstance;
    hSession->hWorkChildThread = uv_thread_self();

    uv_poll_t *pollHandle = hSession->pollHandle[STREAM_WORK];
    pollHandle->data = hSession;
    int initResult = uv_poll_init_socket(&hSession->childLoop, pollHandle, hSession->ctrlFd[STREAM_WORK]);
    if (initResult != 0) {
        WRITE_LOG(LOG_FATAL, "SessionWorkThread init pollHandle->loop failed");
        _exit(0);
    }
    uv_poll_start(pollHandle, UV_READABLE, ReadCtrlFromMain);
    // start heartbeat rimer
    HdcSessionBase *hSessionBase = (HdcSessionBase *)hSession->classInstance;
    hSessionBase->StartHeartbeatWork(hSession);
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hSession->sessionId);
    WRITE_LOG(LOG_INFO, "!!!Workthread run begin, sid:%s instance:%s", sessionIdMaskStr.c_str(),
        thisClass->serverOrDaemon ? "server" : "daemon");
    uv_run(&hSession->childLoop, UV_RUN_DEFAULT);  // work pendding
    WRITE_LOG(LOG_INFO, "!!!Workthread run again, sid:%s", sessionIdMaskStr.c_str());
    // main loop has exit
    thisClass->ReChildLoopForSessionClear(hSession);  // work pending again
    hSession->childCleared = true;
    WRITE_LOG(LOG_INFO, "!!!Workthread run finish, sid:%s", sessionIdMaskStr.c_str());
}

// clang-format off
void HdcSessionBase::LogMsg(const uint32_t sessionId, const uint32_t channelId,
                            MessageLevel level, const char *msg, ...)
// clang-format on
{
    va_list vaArgs;
    va_start(vaArgs, msg);
    string log = Base::StringFormat(msg, vaArgs);
    va_end(vaArgs);
    vector<uint8_t> buf;
    buf.push_back(level);
    buf.insert(buf.end(), log.c_str(), log.c_str() + log.size());
    ServerCommand(sessionId, channelId, CMD_KERNEL_ECHO, buf.data(), buf.size());
}

bool HdcSessionBase::NeedNewTaskInfo(const uint16_t command, bool &masterTask)
{
    // referer from HdcServerForClient::DoCommandRemote
    bool ret = false;
    bool taskMasterInit = false;
    masterTask = false;
    switch (command) {
        case CMD_FILE_INIT:
        case CMD_FLASHD_FLASH_INIT:
        case CMD_FLASHD_UPDATE_INIT:
        case CMD_FLASHD_ERASE:
        case CMD_FLASHD_FORMAT:
        case CMD_FORWARD_INIT:
        case CMD_APP_INIT:
        case CMD_APP_UNINSTALL:
        case CMD_APP_SIDELOAD:
            taskMasterInit = true;
            break;
        default:
            break;
    }
    if (!serverOrDaemon &&
            (command == CMD_SHELL_INIT || command == CMD_UNITY_EXECUTE_EX ||
                (command > CMD_UNITY_COMMAND_HEAD && command < CMD_UNITY_COMMAND_TAIL))) {
        // daemon's single side command
        ret = true;
    } else if (command == CMD_KERNEL_WAKEUP_SLAVETASK) {
        // slave tasks
        ret = true;
    } else if (command == CMD_UNITY_BUGREPORT_INIT) {
        ret = true;
    } else if (taskMasterInit) {
        // task init command
        masterTask = true;
        ret = true;
    }
    return ret;
}

bool HdcSessionBase::AddTaskWithRetry(HSession hSession, TaskInformation* hTaskInfo)
{
    int addTaskRetry = 3; // try 3 time
    while (addTaskRetry > 0) {
        if (AdminTask(OP_ADD, hSession, hTaskInfo->channelId, hTaskInfo)) {
            return true;
        }
        sleep(1);
        --addTaskRetry;
    }
    return false;
}

// Heavy and time-consuming work was putted in the new thread to do, and does
// not occupy the main thread
bool HdcSessionBase::DispatchTaskData(HSession hSession, const uint32_t channelId, const uint16_t command,
                                      uint8_t *payload, int payloadSize)
{
    StartTraceScope("HdcSessionBase::DispatchTaskData");
    bool ret = true;
    HTaskInfo hTaskInfo = nullptr;
    bool masterTask = false;
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hSession->sessionId);
    while (true) {
        // Some basic commands do not have a local task constructor. example: Interactive shell, some uinty commands
        if (NeedNewTaskInfo(command, masterTask)) {
            WRITE_LOG(LOG_INFO, "New HTaskInfo cid:%u sid:%s command:%u", channelId,
                sessionIdMaskStr.c_str(), command);
            hTaskInfo = new(std::nothrow) TaskInformation();
            if (hTaskInfo == nullptr) {
                WRITE_LOG(LOG_FATAL, "DispatchTaskData new hTaskInfo failed");
                break;
            }
            hTaskInfo->channelId = channelId;
            hTaskInfo->sessionId = hSession->sessionId;
            hTaskInfo->runLoop = &hSession->childLoop;
            hTaskInfo->runLoopStatus = &hSession->childLoopStatus;
            hTaskInfo->serverOrDaemon = serverOrDaemon;
            hTaskInfo->masterSlave = masterTask;
            hTaskInfo->closeRetryCount = 0;
            hTaskInfo->channelTask = false;
            hTaskInfo->isCleared = false;

            if (!AddTaskWithRetry(hSession, hTaskInfo)) {
#ifndef HDC_HOST
                LogMsg(hTaskInfo->sessionId, hTaskInfo->channelId,
                    MSG_FAIL, "hdc thread pool busy, may cause reset later");
#endif
                delete hTaskInfo;
                hTaskInfo = nullptr;
                ret = false;
                break;
            }
        } else {
            hTaskInfo = AdminTask(OP_QUERY, hSession, channelId, nullptr);
        }
        if (!hTaskInfo || hTaskInfo->taskStop || hTaskInfo->taskFree) {
            WRITE_LOG(LOG_WARN, "hTaskInfo null cid:%u sid:%s command:%u", channelId,
                sessionIdMaskStr.c_str(), command);
            break;
        }
        ret = RedirectToTask(hTaskInfo, hSession, channelId, command, payload, payloadSize);
        break;
    }
    return ret;
}

void HdcSessionBase::PostStopInstanceMessage(bool restart)
{
    PushAsyncMessage(0, ASYNC_STOP_MAINLOOP, nullptr, 0);
    WRITE_LOG(LOG_INFO, "StopDaemon has sended restart %d", restart);
    wantRestart = restart;
}

void HdcSessionBase::ParsePeerSupportFeatures(HSession &hSession, std::map<std::string, std::string> &tlvMap)
{
    if (hSession == nullptr) {
        WRITE_LOG(LOG_FATAL, "Invalid paramter, hSession is null");
        return;
    }

    if (tlvMap.find(TAG_SUPPORT_FEATURE) != tlvMap.end()) {
        std::vector<std::string> features;
        WRITE_LOG(LOG_INFO, "peer support features are %s for session %s",
            tlvMap[TAG_SUPPORT_FEATURE].c_str(), Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        Base::SplitString(tlvMap[TAG_SUPPORT_FEATURE], ",", features);
        hSession->heartbeat.SetSupportHeartbeat(Base::IsSupportFeature(features, FEATURE_HEARTBEAT));
#ifdef HDC_SUPPORT_ENCRYPT_TCP
        hSession->supportEncrypt = Base::IsSupportFeature(features, FEATURE_ENCRYPT_TCP);
#endif
#ifndef HDC_HOST
        int connectValidationStatus = HdcValidation::GetConnectValidationParam();
        if (connectValidationStatus == VALIDATION_HDC_DAEMON ||
            connectValidationStatus == VALIDATION_HDC_HOST_AND_DAEMON) {
            hSession->supportConnValidation = Base::IsSupportFeature(features, FEATURE_CONN_VALIDATION);
        }
#endif
        HdcStatisticReporter::GetInstance().SetConnectInfo(features);
    }
}

std::string HdcSessionBase::GetOs()
{
#ifdef _WIN32
    return "win";
#elif defined(HOST_LINUX)
    return "linux";
#elif defined(HOST_MAC)
    return "mac";
#elif defined(__OHOS__)
    return "oh";
#else
    return "";
#endif
}
}  // namespace Hdc
