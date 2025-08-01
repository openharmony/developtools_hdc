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
#include "channel.h"
namespace Hdc {
HdcChannelBase::HdcChannelBase(const bool serverOrClient, const string &addrString, uv_loop_t *loopMainIn)
    : loopMainStatus(loopMainIn, "ChannelBaseMainLoop")
{
    SetChannelTCPString(addrString);
    isServerOrClient = serverOrClient;
    loopMain = loopMainIn;
    loopMainStatus.StartReportTimer();
    threadChanneMain = uv_thread_self();
    uv_rwlock_init(&mainAsync);
    uv_async_init(loopMain, &asyncMainLoop, MainAsyncCallback);
    uv_rwlock_init(&lockMapChannel);
    queuedPackages.store(0);
}

HdcChannelBase::~HdcChannelBase()
{
    ClearChannels();
    // clear
    if (!uv_is_closing((uv_handle_t *)&asyncMainLoop)) {
        uv_close((uv_handle_t *)&asyncMainLoop, nullptr);
    }

    uv_rwlock_destroy(&mainAsync);
    uv_rwlock_destroy(&lockMapChannel);
}

vector<uint8_t> HdcChannelBase::GetChannelHandshake(string &connectKey) const
{
    vector<uint8_t> ret;
    struct ChannelHandShake handshake = {};
    if (strcpy_s(handshake.banner, sizeof(handshake.banner), HANDSHAKE_MESSAGE.c_str()) != EOK) {
        return ret;
    }
    if (strcpy_s(handshake.connectKey, sizeof(handshake.connectKey), connectKey.c_str()) != EOK) {
        return ret;
    }
    ret.insert(ret.begin(), (uint8_t *)&handshake, (uint8_t *)&handshake + sizeof(ChannelHandShake));
    return ret;
}

bool HdcChannelBase::SetChannelTCPString(const string &addrString)
{
    bool ret = false;
    while (true) {
        if (addrString.find(":") == string::npos) {
            break;
        }
        std::size_t found = addrString.find_last_of(":");
        if (found == string::npos) {
            break;
        }

        string host = addrString.substr(0, found);
        string port = addrString.substr(found + 1);

        channelPort = std::atoi(port.c_str());
        sockaddr_in addrv4;
        sockaddr_in6 addrv6;
        if (!channelPort) {
            break;
        }

        if (uv_ip6_addr(host.c_str(), channelPort, &addrv6) != 0 &&
            uv_ip4_addr(host.c_str(), channelPort, &addrv4) != 0) {
            break;
        }
        channelHost = host;
        channelHostPort = addrString;
        ret = true;
        break;
    }
    if (!ret) {
        channelPort = 0;
        channelHost = STRING_EMPTY;
        channelHostPort = STRING_EMPTY;
    }
    return ret;
}

void HdcChannelBase::ClearChannels()
{
    for (auto v : mapChannel) {
        HChannel hChannel = (HChannel)v.second;
        if (!hChannel->isDead) {
            FreeChannel(hChannel->channelId);
        }
    }
}

void HdcChannelBase::WorkerPendding()
{
    WRITE_LOG(LOG_DEBUG, "Begin host channel pendding");
    uv_run(loopMain, UV_RUN_DEFAULT);
    uv_loop_close(loopMain);
}

void HdcChannelBase::ReadStream(uv_stream_t *tcp, ssize_t nread, const uv_buf_t *buf)
{
    StartTraceScope("HdcChannelBase::ReadStream");
    int size = 0;
    int indexBuf = 0;
    int childRet = 0;
    bool needExit = false;
    HChannel hChannel = (HChannel)tcp->data;
    HdcChannelBase *thisClass = (HdcChannelBase *)hChannel->clsChannel;
    uint32_t channelId = hChannel->channelId;
    CALLSTAT_GUARD(*(hChannel->loopStatus), tcp->loop, "HdcChannelBase::ReadStream");

    if (nread == UV_ENOBUFS) {
        WRITE_LOG(LOG_FATAL, "ReadStream nobufs channelId:%u", channelId);
#ifdef HDC_HOST
        char buffer[BUF_SIZE_DEFAULT] = { 0 };
        uv_strerror_r(nread, buffer, BUF_SIZE_DEFAULT);
        thisClass->FillChannelResult(hChannel, false, buffer);
#endif
        return;
    } else if (nread == 0) {
        // maybe just after accept, second client req
        WRITE_LOG(LOG_DEBUG, "ReadStream idle read channelId:%u", channelId);
        return;
    } else if (nread < 0) {
        Base::TryCloseHandle((uv_handle_t *)tcp);
        constexpr int bufSize = 1024;
        char buffer[bufSize] = { 0 };
        uv_err_name_r(nread, buffer, bufSize);
        WRITE_LOG(LOG_DEBUG, "ReadStream channelId:%u failed:%s", channelId, buffer);
#ifdef HDC_HOST
        thisClass->FillChannelResult(hChannel, false, buffer);
#endif
        needExit = true;
        goto Finish;
    } else {
        hChannel->availTailIndex += nread;
    }
    while (hChannel->availTailIndex > DWORD_SERIALIZE_SIZE) {
        size = ntohl(*reinterpret_cast<uint32_t *>(hChannel->ioBuf + indexBuf));  // big endian
        if (size <= 0 || static_cast<uint32_t>(size) > HDC_BUF_MAX_BYTES) {
            WRITE_LOG(LOG_FATAL, "ReadStream size:%d channelId:%u", size, channelId);
#ifdef HDC_HOST
            thisClass->FillChannelResult(hChannel, false,
                "parse error: size field is too big");
#endif
            needExit = true;
            break;
        }
        if (hChannel->availTailIndex - DWORD_SERIALIZE_SIZE < size) {
            break;
        }
        childRet = thisClass->ReadChannel(hChannel, reinterpret_cast<uint8_t *>(hChannel->ioBuf) +
                                          DWORD_SERIALIZE_SIZE + indexBuf, size);
        if (childRet < 0) {
            WRITE_LOG(LOG_WARN, "ReadStream childRet:%d channelId:%u keepAlive:%d",
                childRet, channelId, hChannel->keepAlive);
            if (!hChannel->keepAlive) {
                needExit = true;
                break;
            }
        }
        // update io
        hChannel->availTailIndex -= (DWORD_SERIALIZE_SIZE + size);
        indexBuf += DWORD_SERIALIZE_SIZE + size;
    }
    if (indexBuf > 0 && hChannel->availTailIndex > 0) {
        if (memmove_s(hChannel->ioBuf, hChannel->bufSize, hChannel->ioBuf + indexBuf, hChannel->availTailIndex)) {
            needExit = true;
            goto Finish;
        }
    }

Finish:
    if (needExit) {
        thisClass->FreeChannel(hChannel->channelId);
        WRITE_LOG(LOG_DEBUG, "Read Stream needExit, FreeChannel finish channelId:%u", channelId);
    } else {
#ifdef HDC_HOST
      hChannel->isSuccess = (hChannel->faultInfo.size() == 0);
#endif
    }
}

void HdcChannelBase::FileCmdWriteCallback(uv_write_t *req, int status)
{
#ifdef HDC_HOST
        HChannel hChannel = (HChannel)req->handle->data;
        HdcChannelBase *thisClass = (HdcChannelBase *)hChannel->clsChannel;
        thisClass->queuedPackages.fetch_sub(1, std::memory_order_relaxed);
#endif
    WriteCallback(req, status);
}

void HdcChannelBase::WriteCallback(uv_write_t *req, int status)
{
    HChannel hChannel = (HChannel)req->handle->data;
    --hChannel->ref;
    HdcChannelBase *thisClass = (HdcChannelBase *)hChannel->clsChannel;
    CALLSTAT_GUARD(*(hChannel->loopStatus), req->handle->loop, "HdcChannelBase::WriteCallback");
    if (status < 0) {
        WRITE_LOG(LOG_WARN, "WriteCallback status:%d", status);
        hChannel->writeFailedTimes++;
        Base::TryCloseHandle((uv_handle_t *)req->handle);
        if (!hChannel->isDead && !hChannel->ref) {
            thisClass->FreeChannel(hChannel->channelId);
        }
    }
    delete[]((uint8_t *)req->data);
    delete req;
}

void HdcChannelBase::AsyncMainLoopTask(uv_idle_t *handle)
{
    AsyncParam *param = (AsyncParam *)handle->data;
    HdcChannelBase *thisClass = (HdcChannelBase *)param->thisClass;
    CALLSTAT_GUARD(thisClass->loopMainStatus, handle->loop, "HdcChannelBase::AsyncMainLoopTask");
    switch (param->method) {
        case ASYNC_FREE_CHANNEL: {
            // alloc/release should pair in main thread.
            thisClass->FreeChannel(param->sid);
            break;
        }
        default:
            break;
    }
    if (param->data) {
        delete[]((uint8_t *)param->data);
    }
    delete param;
    uv_close((uv_handle_t *)handle, Base::CloseIdleCallback);
}

// multiple uv_async_send() calls may be merged by libuv，so not each call will yield callback as expected.
// eg: if uv_async_send() 5 times before callback calling，it will be called only once.
// if uv_async_send() is called again after callback calling, it will be called again.
void HdcChannelBase::MainAsyncCallback(uv_async_t *handle)
{
    HdcChannelBase *thisClass = (HdcChannelBase *)handle->data;
    CALLSTAT_GUARD(thisClass->loopMainStatus, handle->loop, "HdcChannelBase::MainAsyncCallback");
    if (uv_is_closing((uv_handle_t *)thisClass->loopMain)) {
        WRITE_LOG(LOG_WARN, "MainAsyncCallback uv_is_closing loopMain");
        return;
    }
    list<void *>::iterator i;
    list<void *> &lst = thisClass->lstMainThreadOP;
    uv_rwlock_wrlock(&thisClass->mainAsync);
    for (i = lst.begin(); i != lst.end();) {
        AsyncParam *param = (AsyncParam *)*i;
        Base::IdleUvTask(thisClass->loopMain, param, AsyncMainLoopTask);
        i = lst.erase(i);
    }
    uv_rwlock_wrunlock(&thisClass->mainAsync);
}

void HdcChannelBase::PushAsyncMessage(const uint32_t channelId, const uint8_t method, const void *data,
                                      const int dataSize)
{
    if (uv_is_closing((uv_handle_t *)&asyncMainLoop)) {
        WRITE_LOG(LOG_WARN, "PushAsyncMessage uv_is_closing asyncMainLoop");
        return;
    }
    auto param = new AsyncParam();
    if (!param) {
        return;
    }
    param->sid = channelId;  // Borrow SID storage
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

// add commandflag ahead real buf data
void HdcChannelBase::SendChannelWithCmd(HChannel hChannel, const uint16_t commandFlag, uint8_t *bufPtr, const int size)
{
    StartTraceScope("HdcChannelBase::SendChannelWithCmd");
    if (size < 0) {
        WRITE_LOG(LOG_WARN, "SendChannelWithCmd size %d", size);
        return;
    }
    auto data = new uint8_t[size + sizeof(commandFlag)]();
    if (!data) {
        WRITE_LOG(LOG_WARN, "malloc failed");
        return;
    }

    if (memcpy_s(data, size + sizeof(commandFlag), &commandFlag, sizeof(commandFlag))) {
        delete[] data;
        WRITE_LOG(LOG_DEBUG, "memcpy_s failed commandFlag:%u", commandFlag);
        return;
    }

    if (size > 0 && memcpy_s(data + sizeof(commandFlag), size, bufPtr, size)) {
        delete[] data;
        WRITE_LOG(LOG_DEBUG, "memcpy_s bufPtr failed size:%d", size);
        return;
    }

    SendChannel(hChannel, data, size + sizeof(commandFlag), commandFlag);
    delete[] data;
}

void HdcChannelBase::SendWithCmd(const uint32_t channelId, const uint16_t commandFlag, uint8_t *bufPtr, const int size)
{
    StartTraceScope("HdcChannelBase::SendWithCmd");
    HChannel hChannel = reinterpret_cast<HChannel>(AdminChannel(OP_QUERY_REF, channelId, nullptr));
    if (!hChannel) {
        WRITE_LOG(LOG_FATAL, "SendWithCmd hChannel nullptr channelId:%u", channelId);
        return;
    }
    do {
        if (hChannel->isDead) {
            WRITE_LOG(LOG_FATAL, "SendWithCmd isDead channelId:%u", channelId);
            break;
        }
        SendChannelWithCmd(hChannel, commandFlag, bufPtr, size);
    } while (false);
    --hChannel->ref;
}

void HdcChannelBase::SendChannel(HChannel hChannel, uint8_t *bufPtr, const int size, const uint16_t commandFlag)
{
    StartTraceScope("HdcChannelBase::SendChannel");
    uv_stream_t *sendStream = nullptr;
    int sizeNewBuf = size + DWORD_SERIALIZE_SIZE;
    auto data = new uint8_t[sizeNewBuf]();
    if (!data) {
        WRITE_LOG(LOG_DEBUG, "new data nullptr sizeNewBuf:%d", sizeNewBuf);
        return;
    }
    *reinterpret_cast<uint32_t *>(data) = htonl(size);  // big endian
    if (memcpy_s(data + DWORD_SERIALIZE_SIZE, sizeNewBuf - DWORD_SERIALIZE_SIZE, bufPtr, size)) {
        delete[] data;
        WRITE_LOG(LOG_DEBUG, "memcpy_s failed size:%d", size);
        return;
    }

#ifdef HOST_OHOS
    if (hChannel->hWorkThread == uv_thread_self()) {
        if (!hChannel->isUds) {
            sendStream = (uv_stream_t *)&hChannel->hWorkTCP;
        } else {
            sendStream = (uv_stream_t *)&hChannel->hWorkUds;
        }
    } else {
        if (!hChannel->isUds) {
            sendStream = (uv_stream_t *)&hChannel->hChildWorkTCP;
        } else {
            sendStream = (uv_stream_t *)&hChannel->hChildWorkUds;
        }
    }
#else
    if (hChannel->hWorkThread == uv_thread_self()) {
        sendStream = (uv_stream_t *)&hChannel->hWorkTCP;
    } else {
        sendStream = (uv_stream_t *)&hChannel->hChildWorkTCP;
    }
#endif
    int rc = -1;
    if (!uv_is_closing((const uv_handle_t *)sendStream) && uv_is_writable(sendStream)) {
        ++hChannel->ref;
        if (commandFlag == CMD_FILE_DATA || commandFlag == CMD_APP_DATA) {
            rc = Base::SendToStreamEx(sendStream, data, sizeNewBuf, nullptr, (void *)FileCmdWriteCallback, data);
        } else {
            rc = Base::SendToStreamEx(sendStream, data, sizeNewBuf, nullptr, (void *)WriteCallback, data);
        }
    }
    if (rc < 0) {
        WRITE_LOG(LOG_WARN, "send data failed channelId:%u sizeNewBuf:%d", hChannel->channelId, sizeNewBuf);
        delete[] data;
    }
}

// works only in current working thread
void HdcChannelBase::Send(const uint32_t channelId, uint8_t *bufPtr, const int size)
{
    StartTraceScope("HdcChannelBase::Send");
    HChannel hChannel = reinterpret_cast<HChannel>(AdminChannel(OP_QUERY_REF, channelId, nullptr));
    if (!hChannel) {
        WRITE_LOG(LOG_FATAL, "Send hChannel nullptr channelId:%u", channelId);
        return;
    }
    do {
        if (hChannel->isDead) {
            WRITE_LOG(LOG_FATAL, "Send isDead channelId:%u", channelId);
            break;
        }
        SendChannel(hChannel, bufPtr, size);
    } while (false);
    --hChannel->ref;
}

void HdcChannelBase::AllocCallback(uv_handle_t *handle, size_t sizeWanted, uv_buf_t *buf)
{
    HChannel context = (HChannel)handle->data;
    Base::ReallocBuf(&context->ioBuf, &context->bufSize, Base::GetMaxBufSize() * BUF_EXTEND_SIZE);
    buf->base = (char *)context->ioBuf + context->availTailIndex;
    int size = context->bufSize - context->availTailIndex;
    buf->len = std::min(size, static_cast<int>(sizeWanted));
}

uint32_t HdcChannelBase::GetChannelPseudoUid()
{
    uint32_t uid = 0;
    do {
        uid = Base::GetSecureRandom();
    } while (AdminChannel(OP_QUERY, uid, nullptr) != nullptr);
    return uid;
}

#ifdef HOST_OHOS
uint32_t HdcChannelBase::MallocChannel(HChannel *hOutChannel)
{
#ifdef CONFIG_USE_JEMALLOC_DFX_INIF
    mallopt(M_DELAYED_FREE, M_DELAYED_FREE_DISABLE);
    mallopt(M_SET_THREAD_CACHE, M_THREAD_CACHE_DISABLE);
#endif
    auto hChannel = new HdcChannel();
    if (!hChannel || !(*hOutChannel)) {
        WRITE_LOG(LOG_FATAL, "malloc channel failed");
        return 0;
    }
    hChannel->isUds = (*hOutChannel)->isUds;
    hChannel->stdinTty.data = nullptr;
    hChannel->stdoutTty.data = nullptr;
    uint32_t channelId = GetChannelPseudoUid();
    if (isServerOrClient) {
        hChannel->serverOrClient = isServerOrClient;
        ++channelId;  // Use different value for serverForClient&client in per process
    }
    if (!hChannel->isUds) {
        int rc = uv_tcp_init(loopMain, &hChannel->hWorkTCP);
        if (rc < 0) {
            WRITE_LOG(LOG_FATAL, "MallocChannel uv_tcp_init failed, rc:%d cid:%u", rc, channelId);
        }
        hChannel->hWorkTCP.data = hChannel;
        (void)memset_s(&hChannel->hChildWorkTCP, sizeof(hChannel->hChildWorkTCP), 0, sizeof(uv_tcp_t));
    } else {
        int rc = uv_pipe_init(loopMain, &hChannel->hWorkUds, 0);
        if (rc < 0) {
            WRITE_LOG(LOG_FATAL, "MallocChannel uv_pipe_init failed, rc:%d cid:%u", rc, channelId);
        }
        hChannel->hWorkUds.data = hChannel;
        (void)memset_s(&hChannel->hChildWorkUds, sizeof(hChannel->hChildWorkUds), 0, sizeof(uv_pipe_t));
    }
    ++hChannel->uvHandleRef;
    hChannel->hWorkThread = uv_thread_self();
    hChannel->clsChannel = this;
    hChannel->channelId = channelId;
    hChannel->loopStatus = &loopMainStatus;
    AdminChannel(OP_ADD, channelId, hChannel);
    delete *hOutChannel;
    *hOutChannel = hChannel;
    WRITE_LOG(isServerOrClient ? LOG_INFO : LOG_DEBUG, "Mallocchannel:%u", channelId);
    return channelId;
}
#else
uint32_t HdcChannelBase::MallocChannel(HChannel *hOutChannel)
{
#ifdef CONFIG_USE_JEMALLOC_DFX_INIF
    mallopt(M_DELAYED_FREE, M_DELAYED_FREE_DISABLE);
    mallopt(M_SET_THREAD_CACHE, M_THREAD_CACHE_DISABLE);
#endif
    auto hChannel = new HdcChannel();
    if (!hChannel) {
        WRITE_LOG(LOG_FATAL, "malloc channel failed");
        return 0;
    }
    hChannel->stdinTty.data = nullptr;
    hChannel->stdoutTty.data = nullptr;
    uint32_t channelId = GetChannelPseudoUid();
    if (isServerOrClient) {
        hChannel->serverOrClient = isServerOrClient;
        ++channelId;  // Use different value for serverForClient&client in per process
    }
    int rc = uv_tcp_init(loopMain, &hChannel->hWorkTCP);
    if (rc < 0) {
        WRITE_LOG(LOG_FATAL, "MallocChannel uv_tcp_init failed, rc:%d cid:%u", rc, channelId);
    }

    ++hChannel->uvHandleRef;
    hChannel->hWorkThread = uv_thread_self();
    hChannel->hWorkTCP.data = hChannel;
    hChannel->clsChannel = this;
    hChannel->channelId = channelId;
    hChannel->loopStatus = &loopMainStatus;
    (void)memset_s(&hChannel->hChildWorkTCP, sizeof(hChannel->hChildWorkTCP), 0, sizeof(uv_tcp_t));
    AdminChannel(OP_ADD, channelId, hChannel);
    *hOutChannel = hChannel;
    WRITE_LOG(isServerOrClient ? LOG_INFO : LOG_DEBUG, "Mallocchannel:%u", channelId);
    return channelId;
}
#endif

// work when libuv-handle at struct of HdcSession has all callback finished
void HdcChannelBase::FreeChannelFinally(uv_idle_t *handle)
{
    HChannel hChannel = (HChannel)handle->data;
    HdcChannelBase *thisClass = (HdcChannelBase *)hChannel->clsChannel;
    if (hChannel->uvHandleRef > 0) {
        if (hChannel->serverOrClient) {
            WRITE_LOG(LOG_INFO, "FreeChannelFinally uvHandleRef:%d channelId:%u sid:%u",
                hChannel->uvHandleRef, hChannel->channelId, hChannel->targetSessionId);
        } else {
            WRITE_LOG(LOG_DEBUG, "FreeChannelFinally uvHandleRef:%d channelId:%u sid:%u",
                hChannel->uvHandleRef, hChannel->channelId, hChannel->targetSessionId);
        }
        return;
    }
    thisClass->NotifyInstanceChannelFree(hChannel);
#ifdef HDC_HOST
    hChannel->endTime = Base::GetRuntimeMSec();
    if (hChannel->serverOrClient) {
        thisClass->AdminChannel(OP_PRINT, hChannel->channelId, nullptr);
    }
#endif
    thisClass->AdminChannel(OP_REMOVE, hChannel->channelId, nullptr);

    if (!hChannel->serverOrClient) {
        WRITE_LOG(LOG_DEBUG, "!!!FreeChannelFinally channelId:%u sid:%u finish",
            hChannel->channelId, hChannel->targetSessionId);
        uv_stop(thisClass->loopMain);
    } else {
        WRITE_LOG(LOG_INFO, "!!!FreeChannelFinally channelId:%u sid:%u finish",
            hChannel->channelId, hChannel->targetSessionId);
    }
#ifdef HDC_HOST
    auto deleteChannel = [](uv_handle_t *handle) -> void {
        if (handle->data == nullptr) {
            return;
        }
        HChannel hChannel = reinterpret_cast<HChannel>(handle->data);
        delete hChannel;
    };
    Base::TryCloseHandle((const uv_handle_t *)&hChannel->hChildWorkTCP, true, deleteChannel);
#else
    delete hChannel;
#endif
    Base::TryCloseHandle((const uv_handle_t *)handle, Base::CloseIdleCallback);
}

void HdcChannelBase::FreeChannelContinue(HChannel hChannel)
{
    StartTraceScope("HdcChannelBase::FreeChannelContinue");
    auto closeChannelHandle = [](uv_handle_t *handle) -> void {
        if (handle->data == nullptr) {
            WRITE_LOG(LOG_DEBUG, "FreeChannelContinue handle->data is nullptr");
            return;
        }
        HChannel channel = reinterpret_cast<HChannel>(handle->data);
        --channel->uvHandleRef;
        Base::TryCloseHandle((uv_handle_t *)handle);
    };
    hChannel->availTailIndex = 0;
    if (hChannel->ioBuf) {
        delete[] hChannel->ioBuf;
        hChannel->ioBuf = nullptr;
    }
    if (!hChannel->serverOrClient) {
        Base::TryCloseHandle((uv_handle_t *)&hChannel->stdinTty, closeChannelHandle);
        Base::TryCloseHandle((uv_handle_t *)&hChannel->stdoutTty, closeChannelHandle);
    }
#ifdef HOST_OHOS
    if (!hChannel->isUds) {
        if (uv_is_closing((const uv_handle_t *)&hChannel->hWorkTCP)) {
            --hChannel->uvHandleRef;
        } else {
            Base::TryCloseHandle((uv_handle_t *)&hChannel->hWorkTCP, closeChannelHandle);
        }
    } else {
        if (uv_is_closing((const uv_handle_t *)&hChannel->hWorkUds)) {
            --hChannel->uvHandleRef;
        } else {
            Base::TryCloseHandle((uv_handle_t *)&hChannel->hWorkUds, closeChannelHandle);
        }
    }
#else
    if (uv_is_closing((const uv_handle_t *)&hChannel->hWorkTCP)) {
        --hChannel->uvHandleRef;
    } else {
        Base::TryCloseHandle((uv_handle_t *)&hChannel->hWorkTCP, closeChannelHandle);
    }
#endif
    Base::IdleUvTask(loopMain, hChannel, FreeChannelFinally);
}

#ifdef HOST_OHOS
void HdcChannelBase::FreeChannelOpeate(uv_timer_t *handle)
{
    StartTraceScope("HdcChannelBase::FreeChannelOpeate");
    HChannel hChannel = (HChannel)handle->data;
    HdcChannelBase *thisClass = (HdcChannelBase *)hChannel->clsChannel;
    if (hChannel->ref > 0) {
        return;
    }
    thisClass->DispMntnInfo(hChannel);
    if (hChannel->hChildWorkTCP.loop || hChannel->hChildWorkUds.loop) {
        auto ctrl = HdcSessionBase::BuildCtrlString(SP_DEATCH_CHANNEL, hChannel->channelId, nullptr, 0);
        bool ret = thisClass->ChannelSendSessionCtrlMsg(ctrl, hChannel->targetSessionId);
        if (!ret) {
            WRITE_LOG(LOG_WARN, "FreeChannelOpeate deatch failed channelId:%u sid:%u",
                hChannel->channelId, hChannel->targetSessionId);
            hChannel->childCleared = true;
        }
        auto callbackCheckFreeChannelContinue = [](uv_timer_t *handle) -> void {
            HChannel hChannel = (HChannel)handle->data;
            HdcChannelBase *thisClass = (HdcChannelBase *)hChannel->clsChannel;
            if (!hChannel->childCleared) {
                WRITE_LOG(LOG_WARN, "FreeChannelOpeate childCleared:%d channelId:%u sid:%u",
                    hChannel->childCleared, hChannel->channelId, hChannel->targetSessionId);
                return;
            }
            Base::TryCloseHandle((uv_handle_t *)handle, Base::CloseTimerCallback);
            thisClass->FreeChannelContinue(hChannel);
        };
        Base::TimerUvTask(thisClass->loopMain, hChannel, callbackCheckFreeChannelContinue);
    } else {
        thisClass->FreeChannelContinue(hChannel);
    }
    Base::TryCloseHandle((uv_handle_t *)handle, Base::CloseTimerCallback);
}
#else
void HdcChannelBase::FreeChannelOpeate(uv_timer_t *handle)
{
    StartTraceScope("HdcChannelBase::FreeChannelOpeate");
    HChannel hChannel = (HChannel)handle->data;
    HdcChannelBase *thisClass = (HdcChannelBase *)hChannel->clsChannel;
    if (hChannel->ref > 0) {
        return;
    }
    thisClass->DispMntnInfo(hChannel);
    if (hChannel->hChildWorkTCP.loop) {
        auto ctrl = HdcSessionBase::BuildCtrlString(SP_DEATCH_CHANNEL, hChannel->channelId, nullptr, 0);
        bool ret = thisClass->ChannelSendSessionCtrlMsg(ctrl, hChannel->targetSessionId);
        if (!ret) {
            WRITE_LOG(LOG_WARN, "FreeChannelOpeate deatch failed channelId:%u sid:%u",
                hChannel->channelId, hChannel->targetSessionId);
            hChannel->childCleared = true;
        }
        auto callbackCheckFreeChannelContinue = [](uv_timer_t *handle) -> void {
            HChannel hChannel = (HChannel)handle->data;
            HdcChannelBase *thisClass = (HdcChannelBase *)hChannel->clsChannel;
            if (!hChannel->childCleared) {
                WRITE_LOG(LOG_WARN, "FreeChannelOpeate childCleared:%d channelId:%u sid:%u",
                    hChannel->childCleared, hChannel->channelId, hChannel->targetSessionId);
                return;
            }
            Base::TryCloseHandle((uv_handle_t *)handle, Base::CloseTimerCallback);
            thisClass->FreeChannelContinue(hChannel);
        };
        Base::TimerUvTask(thisClass->loopMain, hChannel, callbackCheckFreeChannelContinue);
    } else {
        thisClass->FreeChannelContinue(hChannel);
    }
    Base::TryCloseHandle((uv_handle_t *)handle, Base::CloseTimerCallback);
}
#endif

void HdcChannelBase::FreeChannel(const uint32_t channelId)
{
    StartTraceScope("HdcChannelBase::FreeChannel");
    if (threadChanneMain != uv_thread_self()) {
        PushAsyncMessage(channelId, ASYNC_FREE_CHANNEL, nullptr, 0);
        WRITE_LOG(LOG_INFO, "FreeChannel not uv_thread_self channelid:%u", channelId);
        return;
    }
    HChannel hChannel = AdminChannel(OP_QUERY, channelId, nullptr);
    do {
        if (!hChannel || hChannel->isDead) {
            WRITE_LOG(LOG_WARN, "FreeChannel hChannel nullptr or isDead channelid:%u", channelId);
            break;
        }
        WRITE_LOG(LOG_DEBUG, "Begin to free channel, channelid:%u", channelId);
        Base::TimerUvTask(loopMain, hChannel, FreeChannelOpeate, MINOR_TIMEOUT);  // do immediately
        hChannel->isDead = true;
    } while (false);
}

#ifdef HDC_HOST
void HdcChannelBase::PrintChannel(const uint32_t channelId)
{
    uv_rwlock_rdlock(&lockMapChannel);
    for (auto v : mapChannel) {
        HChannel hChannel = (HChannel)v.second;
        if (hChannel->channelId == channelId) {
            auto str = hChannel->ToDisplayChannelStr();
            WRITE_LOG(LOG_INFO, "%s", str.c_str());
            break;
        }
    }
    uv_rwlock_rdunlock(&lockMapChannel);
}
#endif

HChannel HdcChannelBase::AdminChannel(const uint8_t op, const uint32_t channelId, HChannel hInput)
{
    StartTraceScope("HdcChannelBase::AdminChannel");
    HChannel hRet = nullptr;
    switch (op) {
        case OP_PRINT:
#ifdef HDC_HOST
            PrintChannel(channelId);
#endif
            break;
        case OP_ADD:
            uv_rwlock_wrlock(&lockMapChannel);
            mapChannel[channelId] = hInput;
            uv_rwlock_wrunlock(&lockMapChannel);
            break;
        case OP_REMOVE:
            uv_rwlock_wrlock(&lockMapChannel);
            mapChannel.erase(channelId);
            uv_rwlock_wrunlock(&lockMapChannel);
            break;
        case OP_QUERY:
            uv_rwlock_rdlock(&lockMapChannel);
            if (mapChannel.count(channelId)) {
                hRet = mapChannel[channelId];
            }
            uv_rwlock_rdunlock(&lockMapChannel);
            break;
        case OP_QUERY_REF:
            uv_rwlock_wrlock(&lockMapChannel);
            if (mapChannel.count(channelId)) {
                hRet = mapChannel[channelId];
                ++hRet->ref;
            }
            uv_rwlock_wrunlock(&lockMapChannel);
            break;
        case OP_UPDATE:
            uv_rwlock_wrlock(&lockMapChannel);
            // remove old
            mapChannel.erase(channelId);
            mapChannel[hInput->channelId] = hInput;
            uv_rwlock_wrunlock(&lockMapChannel);
            break;
        default:
            break;
    }
    return hRet;
}

void HdcChannelBase::EchoToClient(HChannel hChannel, uint8_t *bufPtr, const int size)
{
    StartTraceScope("HdcChannelBase::EchoToClient");
    uv_stream_t *sendStream = nullptr;
    int sizeNewBuf = size + DWORD_SERIALIZE_SIZE;
    auto data = new uint8_t[sizeNewBuf]();
    if (!data) {
        return;
    }
    *reinterpret_cast<uint32_t *>(data) = htonl(size);
    if (memcpy_s(data + DWORD_SERIALIZE_SIZE, sizeNewBuf - DWORD_SERIALIZE_SIZE, bufPtr, size)) {
        delete[] data;
        return;
    }
    sendStream = (uv_stream_t *)&hChannel->hChildWorkTCP;
    int rc = -1;
    if (!uv_is_closing((const uv_handle_t *)sendStream) && uv_is_writable(sendStream)) {
        ++hChannel->ref;
        rc = Base::SendToStreamEx(sendStream, data, sizeNewBuf, nullptr, (void *)WriteCallback, data);
    }
    if (rc < 0) {
        WRITE_LOG(LOG_WARN, "EchoToClient, channelId:%u is unwritable.", hChannel->channelId);
        delete[] data;
    }
}

void HdcChannelBase::EchoToAllChannelsViaSessionId(uint32_t targetSessionId, const string &echo)
{
    for (auto v : mapChannel) {
        HChannel hChannel = (HChannel)v.second;
        if (!hChannel->isDead && hChannel->targetSessionId == targetSessionId) {
            WRITE_LOG(LOG_INFO, "%s:%u %s", __FUNCTION__, targetSessionId, echo.c_str());
            EchoToClient(hChannel, (uint8_t *)echo.c_str(), echo.size());
        }
    }
}

void HdcChannelBase::DispMntnInfo(HChannel hChannel)
{
    if (!hChannel) {
        WRITE_LOG(LOG_WARN, "prt is null");
        return;
    }
    WRITE_LOG(LOG_DEBUG, "channel info: id:%u isDead:%d ref:%u, writeFailedTimes:%u",
        hChannel->channelId, hChannel->isDead, uint32_t(hChannel->ref), uint32_t(hChannel->writeFailedTimes));
}
}
