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
#include "forward.h"
#include "base.h"

namespace Hdc {
#ifdef HDC_HOST
static string g_forwardListenIp = DEFAULT_SERVER_ADDR_IP;
#endif
HdcForwardBase::HdcForwardBase(HTaskInfo hTaskInfo)
    : HdcTaskBase(hTaskInfo)
{
    fds[0] = -1;
    fds[1] = -1;
}

HdcForwardBase::~HdcForwardBase()
{
    WRITE_LOG(LOG_DEBUG, "~HdcForwardBase channelId:%u", taskInfo->channelId);
};

bool HdcForwardBase::ReadyForRelease()
{
    if (!HdcTaskBase::ReadyForRelease()) {
        WRITE_LOG(LOG_WARN, "not ready for release channelId:%u", taskInfo->channelId);
        return false;
    }
    return true;
}

void HdcForwardBase::StopTask()
{
    ctxPointMutex.lock();
    vector<HCtxForward> ctxs;
    map<uint32_t, HCtxForward>::iterator iter;
    for (iter = mapCtxPoint.begin(); iter != mapCtxPoint.end(); ++iter) {
        HCtxForward ctx = iter->second;
        ctxs.push_back(ctx);
    }
    Base::CloseFd(fds[0]);
    // FREECONTEXT in the STOP is triggered by the other party sector, no longer notifying each other.
    mapCtxPoint.clear();
    ctxPointMutex.unlock();
    for (auto ctx: ctxs) {
        FreeContext(ctx, 0, false);
    }
}

void HdcForwardBase::OnAccept(uv_stream_t *server, HCtxForward ctxClient, uv_stream_t *client)
{
    HCtxForward ctxListen = (HCtxForward)server->data;
    char buf[BUF_SIZE_DEFAULT] = { 0 };
    bool ret = false;
    while (true) {
        if (uv_accept(server, client)) {
            WRITE_LOG(LOG_FATAL, "OnAccept uv_accept failed Listenid:%u type:%d remoteParamenters:%s cid:%u sid:%u",
                ctxListen->id, ctxListen->type, ctxListen->remoteParamenters.c_str(), taskInfo->channelId,
                taskInfo->sessionId);
            break;
        }
        ctxClient->type = ctxListen->type;
        ctxClient->remoteParamenters = ctxListen->remoteParamenters;
        int maxSize = sizeof(buf) - forwardParameterBufSize;
        // clang-format off
        if (snprintf_s(buf + forwardParameterBufSize, maxSize, maxSize - 1, "%s",
                       ctxClient->remoteParamenters.c_str()) < 0) {
            WRITE_LOG(LOG_FATAL, "OnAccept prepare buffer failed Listenid:%u type:%d remotePara:%s cid:%u sid:%u",
                ctxListen->id, ctxListen->type, ctxListen->remoteParamenters.c_str(), taskInfo->channelId,
                taskInfo->sessionId);
            break;
        }
        WRITE_LOG(LOG_INFO, "Forward OnAccept ctxclientid:%u type:%d remoteParamenters:%s cid:%u sid:%u",
            ctxClient->id, ctxClient->type, ctxClient->remoteParamenters.c_str(), taskInfo->channelId,
            taskInfo->sessionId);
        SendToTask(ctxClient->id, CMD_FORWARD_ACTIVE_SLAVE, reinterpret_cast<uint8_t *>(buf),
                   strlen(buf + forwardParameterBufSize) + 9); // 9: pre 8bytes preserve for param bits
        ret = true;
        break;
    }
    if (!ret) {
        FreeContext(ctxClient, 0, false);
    }
}

void HdcForwardBase::ListenCallback(uv_stream_t *server, const int status)
{
    HCtxForward ctxListen = (HCtxForward)server->data;
    HdcForwardBase *thisClass = ctxListen->thisClass;
    uv_stream_t *client = nullptr;
    CALLSTAT_GUARD(*(ctxListen->thisClass->loopTaskStatus), server->loop, "HdcForwardBase::ListenCallback");

    if (status == -1 || !ctxListen->ready) {
        WRITE_LOG(LOG_FATAL, "ListenCallback status:%d id:%u ready:%d cid:%u sid:%u",
            status, ctxListen->id, ctxListen->ready, thisClass->taskInfo->channelId, thisClass->taskInfo->sessionId);
        thisClass->FreeContext(ctxListen, 0, false);
        thisClass->TaskFinish();
        return;
    }
    HCtxForward ctxClient = (HCtxForward)thisClass->MallocContext(true);
    if (!ctxClient) {
        WRITE_LOG(LOG_FATAL, "ListenCallback ctxClient is null, ctxListenid:%u cid:%u sid:%u",
            ctxListen->id, thisClass->taskInfo->channelId, thisClass->taskInfo->sessionId);
        return;
    }
    if (ctxListen->type == FORWARD_TCP) {
        uv_tcp_init(ctxClient->thisClass->loopTask, &ctxClient->tcp);
        client = (uv_stream_t *)&ctxClient->tcp;
    } else {
        // FORWARD_ABSTRACT, FORWARD_RESERVED, FORWARD_FILESYSTEM,
        uv_pipe_init(ctxClient->thisClass->loopTask, &ctxClient->pipe, 0);
        client = (uv_stream_t *)&ctxClient->pipe;
    }
    thisClass->OnAccept(server, ctxClient, client);
}

void *HdcForwardBase::MallocContext(bool masterSlave)
{
    HCtxForward ctx = nullptr;
    if ((ctx = new ContextForward()) == nullptr) {
        return nullptr;
    }
    ctx->id = Base::GetRandomU32();
    ctx->masterSlave = masterSlave;
    ctx->thisClass = this;
    ctx->fd = -1;
    ctx->fdClass = nullptr;
    ctx->tcp.data = ctx;
    ctx->pipe.data = ctx;
    AdminContext(OP_ADD, ctx->id, ctx);
    refCount++;
    return ctx;
}

void HdcForwardBase::FreeContextCallBack(HCtxForward ctx)
{
    Base::DoNextLoop(loopTask, ctx, [this](const uint8_t flag, string &msg, const void *data) {
        HCtxForward ctx = (HCtxForward)data;
        AdminContext(OP_REMOVE, ctx->id, nullptr);
        if (ctx != nullptr) {
            WRITE_LOG(LOG_DEBUG, "Finally to delete id:%u", ctx->id);
            delete ctx;
            ctx = nullptr;
        }
        if (refCount > 0) {
            --refCount;
        }
    });
}

void HdcForwardBase::FreeJDWP(HCtxForward ctx)
{
    Base::CloseFd(ctx->fd);
    if (ctx->fdClass) {
        ctx->fdClass->StopWorkOnThread(false, nullptr);

        auto funcReqClose = [](uv_idle_t *handle) -> void {
            uv_close_cb funcIdleHandleClose = [](uv_handle_t *handle) -> void {
                HCtxForward ctx = (HCtxForward)handle->data;
                ctx->thisClass->FreeContextCallBack(ctx);
                delete (uv_idle_t *)handle;
            };
            HCtxForward context = (HCtxForward)handle->data;
            if (context->fdClass->ReadyForRelease()) {
                delete context->fdClass;
                context->fdClass = nullptr;
                Base::TryCloseHandle((uv_handle_t *)handle, funcIdleHandleClose);
            }
        };
        Base::IdleUvTask(loopTask, ctx, funcReqClose);
    } else {
        auto funcReqClose = [](uv_idle_t *handle) -> void {
            uv_close_cb funcIdleHandleClose = [](uv_handle_t *handle) -> void {
                HCtxForward ctx = (HCtxForward)handle->data;
                ctx->thisClass->FreeContextCallBack(ctx);
                delete (uv_idle_t *)handle;
            };
            Base::TryCloseHandle((uv_handle_t *)handle, funcIdleHandleClose);
        };
        Base::IdleUvTask(loopTask, ctx, funcReqClose);
    }
}

void HdcForwardBase::FreeContext(HCtxForward ctxIn, const uint32_t id, bool bNotifyRemote)
{
    std::lock_guard<std::mutex> lock(ctxFreeMutex);
    HCtxForward ctx = nullptr;
    if (!ctxIn) {
        if (!(ctx = (HCtxForward)AdminContext(OP_QUERY, id, nullptr))) {
            WRITE_LOG(LOG_DEBUG, "FreeContext Query id:%u failed cid:%u sid:%u", id, taskInfo->channelId,
                taskInfo->sessionId);
            return;
        }
    } else {
        ctx = ctxIn;
    }
    WRITE_LOG(LOG_INFO, "FreeContext id:%u, bNotifyRemote:%d, finish:%d cid:%u sid:%u",
        ctx->id, bNotifyRemote, ctx->finish, taskInfo->channelId, taskInfo->sessionId);
    if (ctx->finish) {
        return;
    }
    if (bNotifyRemote) {
        SendToTask(ctx->id, CMD_FORWARD_FREE_CONTEXT, nullptr, 0);
    }
    uv_close_cb funcHandleClose = [](uv_handle_t *handle) -> void {
        HCtxForward ctx = (HCtxForward)handle->data;
        ctx->thisClass->FreeContextCallBack(ctx);
    };
    switch (ctx->type) {
        case FORWARD_TCP:
        case FORWARD_JDWP:
        case FORWARD_ARK:
            Base::TryCloseHandle((uv_handle_t *)&ctx->tcp, true, funcHandleClose);
            break;
        case FORWARD_ABSTRACT:
        case FORWARD_RESERVED:
        case FORWARD_FILESYSTEM:
            Base::TryCloseHandle((uv_handle_t *)&ctx->pipe, true, funcHandleClose);
            break;
        case FORWARD_DEVICE: {
            FreeJDWP(ctx);
            break;
        }
        default:
            break;
    }
    ctx->finish = true;
}

bool HdcForwardBase::SendToTask(const uint32_t cid, const uint16_t command, uint8_t *bufPtr, const int bufSize)
{
    StartTraceScope("HdcForwardBase::SendToTask");
    bool ret = false;
    // usually MAX_SIZE_IOBUF*2 from HdcFileDescriptor maxIO
    if (bufSize > Base::GetMaxBufSizeStable() * BUF_MULTIPLE) {
        WRITE_LOG(LOG_FATAL, "SendToTask bufSize:%d", bufSize);
        return false;
    }
    auto newBuf = new uint8_t[bufSize + BUF_EXTEND_SIZE];
    if (!newBuf) {
        return false;
    }
    *reinterpret_cast<uint32_t *>(newBuf) = htonl(cid);
    if (bufSize > 0 && bufPtr != nullptr && memcpy_s(newBuf + BUF_EXTEND_SIZE, bufSize, bufPtr, bufSize) != EOK) {
        delete[] newBuf;
        return false;
    }
    ret = SendToAnother(command, newBuf, bufSize + BUF_EXTEND_SIZE);
    delete[] newBuf;
    return ret;
}

// Forward flow is small and frequency is fast
void HdcForwardBase::AllocForwardBuf(uv_handle_t *handle, size_t sizeSuggested, uv_buf_t *buf)
{
    size_t size = sizeSuggested;
    if (size > MAX_USBFFS_BULK) {
        size = MAX_USBFFS_BULK;
    }
    buf->base = (char *)new char[size];
    if (buf->base) {
        buf->len = (size > 0) ? (size - 1) : 0;
    } else {
        WRITE_LOG(LOG_WARN, "AllocForwardBuf == null");
    }
}

void HdcForwardBase::ReadForwardBuf(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    HCtxForward ctx = (HCtxForward)stream->data;
    CALLSTAT_GUARD(*(ctx->thisClass->loopTaskStatus), stream->loop, "HdcForwardBase::ReadForwardBuf");
    if (nread < 0) {
        WRITE_LOG(LOG_INFO, "ReadForwardBuf nread:%zd id:%u cid:%u sid:%u", nread, ctx->id,
            ctx->thisClass->taskInfo->channelId, ctx->thisClass->taskInfo->sessionId);
        ctx->thisClass->FreeContext(ctx, 0, true);
        delete[] buf->base;
        return;
    }
    if (nread == 0) {
        WRITE_LOG(LOG_INFO, "ReadForwardBuf nread:0 id:%u cid:%u sid:%u", nread, ctx->id,
            ctx->thisClass->taskInfo->channelId, ctx->thisClass->taskInfo->sessionId);
        delete[] buf->base;
        return;
    }
    ctx->thisClass->SendToTask(ctx->id, CMD_FORWARD_DATA, (uint8_t *)buf->base, nread);
    // clear
    delete[] buf->base;
}

void HdcForwardBase::ConnectTarget(uv_connect_t *connection, int status)
{
    HCtxForward ctx = (HCtxForward)connection->data;
    HdcForwardBase *thisClass = ctx->thisClass;
    CALLSTAT_GUARD(*(ctx->thisClass->loopTaskStatus), connection->handle->loop, "HdcForwardBase::ConnectTarget");
    delete connection;
    if (status < 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        uv_err_name_r(status, buf, bufSize);
        WRITE_LOG(LOG_WARN, "Forward connect result:%d error:%s", status, buf);
    }
    thisClass->SetupPointContinue(ctx, status);
}

bool HdcForwardBase::CheckNodeInfo(const char *nodeInfo, string as[2])
{
    if (nodeInfo == nullptr) {
        return false;
    }
    string str = nodeInfo;
    size_t strLen = str.size();
    if (strLen < 1) {
        return false;
    }
    size_t pos = str.find(':');
    if (pos != string::npos) {
        if (pos == 0 || pos == strLen - 1) {
            return false;
        }
        as[0] = str.substr(0, pos);
        as[1] = str.substr(pos + 1);
    } else {
        return false;
    }
    if (as[0] == "tcp") {
        if (as[1].size() > std::to_string(MAX_IP_PORT).size()) {
            return false;
        }
        int port = atoi(as[1].c_str());
        if (port <= 0 || port > MAX_IP_PORT) {
            return false;
        }
    }
    return true;
}

bool HdcForwardBase::SetupPointContinue(HCtxForward ctx, int status)
{
    if (ctx->checkPoint) {
        // send to active
        uint8_t flag = status > 0;
        SendToTask(ctx->id, CMD_FORWARD_CHECK_RESULT, &flag, 1);
        FreeContext(ctx, 0, false);
        return true;
    }
    if (status < 0) {
        FreeContext(ctx, 0, true);
        return false;
    }
    // send to active
    if (!SendToTask(ctx->id, CMD_FORWARD_ACTIVE_MASTER, nullptr, 0)) {
        WRITE_LOG(LOG_FATAL, "SetupPointContinue SendToTask failed id:%u", ctx->id);
        FreeContext(ctx, 0, true);
        return false;
    }
    return DoForwardBegin(ctx);
}

bool HdcForwardBase::DetechForwardType(HCtxForward ctxPoint)
{
    string &sFType = ctxPoint->localArgs[0];
    string &sNodeCfg = ctxPoint->localArgs[1];
    // string to enum
    if (sFType == "tcp") {
        ctxPoint->type = FORWARD_TCP;
    } else if (sFType == "dev") {
        ctxPoint->type = FORWARD_DEVICE;
    } else if (sFType == "localabstract") {
        // daemon shell: /system/bin/socat abstract-listen:linux-abstract -
        // daemon shell: /system/bin/socat - abstract-connect:linux-abstract
        // host:   hdc fport tcp:8080 localabstract:linux-abstract
        ctxPoint->type = FORWARD_ABSTRACT;
    } else if (sFType == "localreserved") {
        sNodeCfg = harmonyReservedSocketPrefix + sNodeCfg;
        ctxPoint->type = FORWARD_RESERVED;
    } else if (sFType == "localfilesystem") {
        sNodeCfg = filesystemSocketPrefix + sNodeCfg;
        ctxPoint->type = FORWARD_FILESYSTEM;
    } else if (sFType == "jdwp") {
        ctxPoint->type = FORWARD_JDWP;
    } else if (sFType == "ark") {
        ctxPoint->type = FORWARD_ARK;
    } else {
        return false;
    }
    return true;
}

#ifdef HDC_HOST
void HdcForwardBase::SetForwardListenIP(string &ip)
{
    g_forwardListenIp = ip;
}

bool HdcForwardBase::SetupTcpListenAllIp(HCtxForward ctxPoint, int port, int &rc)
{
    struct sockaddr_in addrv4;
    struct sockaddr_in6 addrv6;
    char buffer[BUF_SIZE_DEFAULT] = { 0 };
    rc = uv_ip6_addr(g_forwardListenIp.c_str(), port, &addrv6);
    if (rc != 0) {
        uv_strerror_r(rc, buffer, BUF_SIZE_DEFAULT);
        WRITE_LOG(LOG_FATAL, "SetupTcpListenAllIp uv_ip6_addr %d %s", rc, buffer);
        return false;
    }
    rc = uv_tcp_bind(&ctxPoint->tcp, (const struct sockaddr *)&addrv6, 0);
    if (rc != 0) {
        WRITE_LOG(LOG_WARN, "SetupTcpListenAllIp uv_tcp_bind ipv6 %d", rc);
        if (rc == -EAFNOSUPPORT) {
            size_t index = g_forwardListenIp.find(IPV4_MAPPING_PREFIX);
            size_t size = IPV4_MAPPING_PREFIX.size();
            if (index == std::string::npos) {
                return false;
            }
            std::string ipv4 = g_forwardListenIp.substr(index + size);
            uv_ip4_addr(ipv4.c_str(), port, &addrv4);
            rc = uv_tcp_bind(&ctxPoint->tcp, (const struct sockaddr *)&addrv4, 0);
            if (rc != 0) {
                uv_strerror_r(rc, buffer, BUF_SIZE_DEFAULT);
                WRITE_LOG(LOG_FATAL, "SetupTcpListenAllIp uv_tcp_bind ipv4 %s failed %d %s",
                          Hdc::MaskString(ipv4).c_str(), rc, buffer);
                return false;
            }
        } else {
            uv_strerror_r(rc, buffer, BUF_SIZE_DEFAULT);
            WRITE_LOG(LOG_FATAL, "SetupTcpListenAllIp uv_tcp_bind %d %s", rc, buffer);
            return false;
        }
    }
    return true;
}
#endif

bool HdcForwardBase::SetupTCPPoint(HCtxForward ctxPoint)
{
    string &sNodeCfg = ctxPoint->localArgs[1];
    int port = atoi(sNodeCfg.c_str());
    ctxPoint->tcp.data = ctxPoint;
    int rc = uv_tcp_init(loopTask, &ctxPoint->tcp);
    if (rc < 0) {
        WRITE_LOG(LOG_FATAL, "SetupTCPPoint uv_tcp_init failed, rc:%d port:%d ctxid:%u cid:%u sid:%u", rc, port,
            ctxPoint->id, taskInfo->channelId, taskInfo->sessionId);
        return false;
    }
    struct sockaddr_in addr;
    if (ctxPoint->masterSlave) {
#ifdef HDC_HOST
        bool ret = SetupTcpListenAllIp(ctxPoint, port, rc);
        WRITE_LOG(LOG_DEBUG, "SetupTcpListenAllIp ret:%d, rc:%d", ret, rc);
#else
        uv_ip4_addr("127.0.0.1", port, &addr); // loop interface
        rc = uv_tcp_bind(&ctxPoint->tcp, (const struct sockaddr *)&addr, 0);
#endif
        if (rc < 0) {
            WRITE_LOG(LOG_FATAL, "SetupTCPPoint master uv_tcp_bind failed, rc:%d port:%d ctxid:%u cid:%u sid:%u",
                rc, port, ctxPoint->id, taskInfo->channelId, taskInfo->sessionId);
            return false;
        }
        WRITE_LOG(LOG_INFO, "SetupTCPPoint master uv_listen port:%d ctxid:%u cid:%u sid:%u", port, ctxPoint->id,
            taskInfo->channelId, taskInfo->sessionId);
        if (uv_listen((uv_stream_t *)&ctxPoint->tcp, UV_LISTEN_LBACKOG, ListenCallback)) {
            ctxPoint->lastError = "TCP Port listen failed at " + sNodeCfg;
            WRITE_LOG(LOG_FATAL, "SetupTCPPoint master uv_listen failed port:%d ctxid:%u cid:%u sid:%u",
                port, ctxPoint->id, taskInfo->channelId, taskInfo->sessionId);
            return false;
        }
    } else {
        uv_ip4_addr("127.0.0.1", port, &addr);  // loop interface
        uv_connect_t *conn = new(std::nothrow) uv_connect_t();
        if (conn == nullptr) {
            WRITE_LOG(LOG_FATAL, "SetupTCPPoint new conn failed port:%d ctxid:%u cid:%u sid:%u", port, ctxPoint->id,
                taskInfo->channelId, taskInfo->sessionId);
            return false;
        }
        conn->data = ctxPoint;
        WRITE_LOG(LOG_INFO, "SetupTCPPoint slave uv_tcp_connect port:%d ctxid:%u cid:%u sid:%u", port, ctxPoint->id,
            taskInfo->channelId, taskInfo->sessionId);
        uv_tcp_connect(conn, (uv_tcp_t *)&ctxPoint->tcp, (const struct sockaddr *)&addr, ConnectTarget);
    }
    return true;
}

bool HdcForwardBase::SetupDevicePoint(HCtxForward ctxPoint)
{
    uint8_t flag = 1;
    string &sNodeCfg = ctxPoint->localArgs[1];
    string resolvedPath = Base::CanonicalizeSpecPath(sNodeCfg);
    if ((ctxPoint->fd = open(resolvedPath.c_str(), O_RDWR)) < 0) {
        ctxPoint->lastError = "Open unix-dev failed";
        flag = -1;
    }
    auto funcRead = [&](const void *a, uint8_t *b, const int c) -> bool {
        HCtxForward ctx = (HCtxForward)a;
        return SendToTask(ctx->id, CMD_FORWARD_DATA, b, c);
    };
    auto funcFinish = [&](const void *a, const bool b, const string c) -> bool {
        HCtxForward ctx = (HCtxForward)a;
        WRITE_LOG(LOG_DEBUG, "funcFinish id:%u ret:%d reason:%s", ctx->id, b, c.c_str());
        FreeContext(ctx, 0, true);
        return false;
    };
    ctxPoint->fdClass = new(std::nothrow) HdcFileDescriptor(loopTask, ctxPoint->fd, ctxPoint, funcRead,
                                                            funcFinish, true);
    if (ctxPoint->fdClass == nullptr) {
        WRITE_LOG(LOG_FATAL, "SetupDevicePoint new ctxPoint->fdClass failed");
        return false;
    }
    SetupPointContinue(ctxPoint, flag);
    return true;
}

bool HdcForwardBase::LocalAbstractConnect(uv_pipe_t *pipe, string &sNodeCfg)
{
    bool abstractRet = false;
#ifndef _WIN32
    int s = 0;
    do {
        if ((s = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
            break;
        }
        fcntl(s, F_SETFD, FD_CLOEXEC);
        struct sockaddr_un addr;
        (void)memset_s(&addr, sizeof(sockaddr_un), 0, sizeof(sockaddr_un));
        size_t  addrLen = sNodeCfg.size() + offsetof(struct sockaddr_un, sun_path) + 1;
        addr.sun_family = AF_LOCAL;
        addr.sun_path[0] = 0;

        if (memcpy_s(addr.sun_path + 1, sizeof(addr.sun_path) - 1, sNodeCfg.c_str(), sNodeCfg.size()) != EOK) {
            break;
        };
        struct timeval timeout = { 3, 0 };
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        if (connect(s, reinterpret_cast<struct sockaddr *>(&addr), addrLen) < 0) {
            WRITE_LOG(LOG_FATAL, "LocalAbstractConnect failed errno:%d", errno);
            break;
        }
        if (uv_pipe_open(pipe, s)) {
            break;
        }
        abstractRet = true;
    } while (false);
    if (!abstractRet) {
        Base::CloseFd(s);
    }
#endif
    return abstractRet;
}

bool HdcForwardBase::SetupFilePoint(HCtxForward ctxPoint)
{
    string &sNodeCfg = ctxPoint->localArgs[1];
    ctxPoint->pipe.data = ctxPoint;
    uv_pipe_init(loopTask, &ctxPoint->pipe, 0);
    if (ctxPoint->masterSlave) {
        if (ctxPoint->type == FORWARD_RESERVED || ctxPoint->type == FORWARD_FILESYSTEM) {
            unlink(sNodeCfg.c_str());
        }
        if (uv_pipe_bind(&ctxPoint->pipe, sNodeCfg.c_str())) {
            ctxPoint->lastError = "Unix pipe bind failed";
            return false;
        }
        if (uv_listen((uv_stream_t *)&ctxPoint->pipe, UV_LISTEN_LBACKOG, ListenCallback)) {
            ctxPoint->lastError = "Unix pipe listen failed";
            return false;
        }
    } else {
        if (ctxPoint->type == FORWARD_ABSTRACT) {
            bool abstractRet = LocalAbstractConnect(&ctxPoint->pipe, sNodeCfg);
            SetupPointContinue(ctxPoint, abstractRet ? 0 : -1);
            if (!abstractRet) {
                ctxPoint->lastError = "LocalAbstractConnect failed";
                return false;
            }
        } else {
            uv_connect_t *connect = new(std::nothrow) uv_connect_t();
            if (connect == nullptr) {
                WRITE_LOG(LOG_FATAL, "SetupFilePoint new connect failed");
                return false;
            }
            connect->data = ctxPoint;
            uv_pipe_connect(connect, &ctxPoint->pipe, sNodeCfg.c_str(), ConnectTarget);
        }
    }
    return true;
}

bool HdcForwardBase::SetupPoint(HCtxForward ctxPoint)
{
    bool ret = true;
    if (!DetechForwardType(ctxPoint)) {
        return false;
    }
    switch (ctxPoint->type) {
        case FORWARD_TCP:
            if (!SetupTCPPoint(ctxPoint)) {
                ret = false;
            };
            break;
#ifndef _WIN32
        case FORWARD_DEVICE:
            if (!SetupDevicePoint(ctxPoint)) {
                ret = false;
            };
            break;
        case FORWARD_JDWP:
            if (!SetupJdwpPoint(ctxPoint)) {
                ret = false;
            };
            break;
        case FORWARD_ABSTRACT:
        case FORWARD_RESERVED:
        case FORWARD_FILESYSTEM:
            if (!SetupFilePoint(ctxPoint)) {
                ret = false;
            };
            break;
#else
        case FORWARD_DEVICE:
        case FORWARD_JDWP:
        case FORWARD_ABSTRACT:
        case FORWARD_RESERVED:
        case FORWARD_FILESYSTEM:
            ctxPoint->lastError = "Not supoort forward-type";
            ret = false;
            break;
#endif
        default:
            ctxPoint->lastError = "Not supoort forward-type";
            ret = false;
            break;
    }
    return ret;
}

bool HdcForwardBase::BeginForward(const string &command, string &sError)
{
    bool ret = false;
    int argc = 0;
    char bufString[BUF_SIZE_SMALL] = "";
    HCtxForward ctxPoint = (HCtxForward)MallocContext(true);
    if (!ctxPoint) {
        WRITE_LOG(LOG_FATAL, "MallocContext failed");
        return false;
    }
    char **argv = Base::SplitCommandToArgs(command.c_str(), &argc);
    if (argv == nullptr) {
        WRITE_LOG(LOG_FATAL, "SplitCommandToArgs failed");
        return false;
    }
    while (true) {
        if (argc < CMD_ARG1_COUNT) {
            break;
        }
        if (strlen(argv[0]) > BUF_SIZE_SMALL || strlen(argv[1]) > BUF_SIZE_SMALL) {
            break;
        }
        if (!CheckNodeInfo(argv[0], ctxPoint->localArgs)) {
            break;
        }
        if (!CheckNodeInfo(argv[1], ctxPoint->remoteArgs)) {
            break;
        }
        ctxPoint->remoteParamenters = argv[1];
        if (!SetupPoint(ctxPoint)) {
            break;
        }

        ret = true;
        break;
    }
    sError = ctxPoint->lastError;
    if (ret) {
        // First 8-byte parameter bit
        int maxBufSize = sizeof(bufString) - forwardParameterBufSize;
        if (snprintf_s(bufString + forwardParameterBufSize, maxBufSize, maxBufSize - 1, "%s", argv[1]) > 0) {
            SendToTask(ctxPoint->id, CMD_FORWARD_CHECK, reinterpret_cast<uint8_t *>(bufString),
                       forwardParameterBufSize + strlen(bufString + forwardParameterBufSize) + 1);
            taskCommand = command;
        }
    }
    delete[](reinterpret_cast<char *>(argv));
    return ret;
}

inline bool HdcForwardBase::FilterCommand(uint8_t *bufCmdIn, uint32_t *idOut, uint8_t **pContentBuf)
{
    *pContentBuf = bufCmdIn + DWORD_SERIALIZE_SIZE;
    *idOut = ntohl(*reinterpret_cast<uint32_t *>(bufCmdIn));
    return true;
}

bool HdcForwardBase::SlaveConnect(uint8_t *bufCmd, const int bufSize, bool bCheckPoint, string &sError)
{
    if (bufSize <= DWORD_SERIALIZE_SIZE + forwardParameterBufSize) {
        WRITE_LOG(LOG_FATAL, "Illegal payloadSize, shorter than forward header");
        return false;
    }
    bool ret = false;
    char *content = nullptr;
    uint32_t idSlaveOld = 0;
    HCtxForward ctxPoint = (HCtxForward)MallocContext(false);
    if (!ctxPoint) {
        WRITE_LOG(LOG_FATAL, "MallocContext failed");
        return false;
    }
    idSlaveOld = ctxPoint->id;
    ctxPoint->checkPoint = bCheckPoint;
    // refresh another id,8byte param
    FilterCommand(bufCmd, &ctxPoint->id, reinterpret_cast<uint8_t **>(&content));
    AdminContext(OP_UPDATE, idSlaveOld, ctxPoint);
    content += forwardParameterBufSize;
    if (!CheckNodeInfo(content, ctxPoint->localArgs)) {
        WRITE_LOG(LOG_FATAL, "SlaveConnect CheckNodeInfo failed content:%s", content);
        goto Finish;
    }
    if (!DetechForwardType(ctxPoint)) {
        WRITE_LOG(LOG_FATAL, "SlaveConnect DetechForwardType failed content:%s", content);
        goto Finish;
    }
    WRITE_LOG(LOG_INFO, "SlaveConnect ctxid:%u type:%d cid:%u sid:%u", ctxPoint->id, ctxPoint->type,
        taskInfo->channelId, taskInfo->sessionId);
    if (ctxPoint->type == FORWARD_ARK) {
        if (ctxPoint->checkPoint) {
            if (!SetupArkPoint(ctxPoint)) {
                sError = ctxPoint->lastError;
                WRITE_LOG(LOG_FATAL, "SlaveConnect SetupArkPoint failed content:%s", content);
                goto Finish;
            }
        } else {
            SetupPointContinue(ctxPoint, 0);
        }
        ret = true;
    } else {
        if (!ctxPoint->checkPoint) {
            if (!SetupPoint(ctxPoint)) {
                sError = ctxPoint->lastError;
                WRITE_LOG(LOG_FATAL, "SlaveConnect SetupPoint failed content:%s", content);
                goto Finish;
            }
        } else {
            SetupPointContinue(ctxPoint, 0);
        }
        ret = true;
    }
Finish:
    if (!ret) {
        FreeContext(ctxPoint, 0, true);
    }
    return ret;
}

bool HdcForwardBase::DoForwardBegin(HCtxForward ctx)
{
    switch (ctx->type) {
        case FORWARD_TCP:
        case FORWARD_JDWP:  // jdwp use tcp ->socketpair->jvm
            uv_tcp_nodelay((uv_tcp_t *)&ctx->tcp, 1);
            uv_read_start((uv_stream_t *)&ctx->tcp, AllocForwardBuf, ReadForwardBuf);
            break;
        case FORWARD_ARK:
            WRITE_LOG(LOG_DEBUG, "DoForwardBegin ark socketpair id:%u fds[0]:%d", ctx->id, fds[0]);
            uv_tcp_init(loopTask, &ctx->tcp);
            uv_tcp_open(&ctx->tcp, fds[0]);
            uv_tcp_nodelay((uv_tcp_t *)&ctx->tcp, 1);
            uv_read_start((uv_stream_t *)&ctx->tcp, AllocForwardBuf, ReadForwardBuf);
            break;
        case FORWARD_ABSTRACT:
        case FORWARD_RESERVED:
        case FORWARD_FILESYSTEM:
            uv_read_start((uv_stream_t *)&ctx->pipe, AllocForwardBuf, ReadForwardBuf);
            break;
        case FORWARD_DEVICE: {
            ctx->fdClass->StartWorkOnThread();
            break;
        }
        default:
            break;
    }
    ctx->ready = true;
    return true;
}

void *HdcForwardBase::AdminContext(const uint8_t op, const uint32_t id, HCtxForward hInput)
{
    ctxPointMutex.lock();
    void *hRet = nullptr;
    map<uint32_t, HCtxForward> &mapCtx = mapCtxPoint;
    switch (op) {
        case OP_ADD:
            mapCtx[id] = hInput;
            break;
        case OP_REMOVE:
            mapCtx.erase(id);
            break;
        case OP_QUERY:
            if (mapCtx.count(id)) {
                hRet = mapCtx[id];
            }
            break;
        case OP_UPDATE:
            mapCtx.erase(id);
            mapCtx[hInput->id] = hInput;
            break;
        default:
            break;
    }
    ctxPointMutex.unlock();
    return hRet;
}

void HdcForwardBase::SendCallbackForwardBuf(uv_write_t *req, int status)
{
    ContextForwardIO *ctxIO = (ContextForwardIO *)req->data;
    HCtxForward ctx = reinterpret_cast<HCtxForward>(ctxIO->ctxForward);
    if (status < 0 && !ctx->finish) {
        WRITE_LOG(LOG_DEBUG, "SendCallbackForwardBuf ctx->type:%d, status:%d finish", ctx->type, status);
        ctx->thisClass->FreeContext(ctx, 0, true);
    }
    delete[] ctxIO->bufIO;
    delete ctxIO;
    delete req;
}

int HdcForwardBase::SendForwardBuf(HCtxForward ctx, uint8_t *bufPtr, const int size)
{
    int nRet = 0;
    if (size > static_cast<int>(HDC_BUF_MAX_BYTES - 1)) {
        WRITE_LOG(LOG_WARN, "SendForwardBuf size:%d > HDC_BUF_MAX_BYTES, ctxId:%u", size, ctx->id);
        return -1;
    }
    if (size <= 0) {
        WRITE_LOG(LOG_WARN, "SendForwardBuf failed size:%d, ctxId:%u", size, ctx->id);
        return -1;
    }
    auto pDynBuf = new(std::nothrow) uint8_t[size];
    if (!pDynBuf) {
        WRITE_LOG(LOG_WARN, "SendForwardBuf new DynBuf failed size:%d, ctxId:%u", size, ctx->id);
        return -1;
    }
    (void)memcpy_s(pDynBuf, size, bufPtr, size);
    if (ctx->type == FORWARD_DEVICE) {
        nRet = ctx->fdClass->WriteWithMem(pDynBuf, size);
    } else {
        auto ctxIO = new ContextForwardIO();
        if (!ctxIO) {
            WRITE_LOG(LOG_WARN, "SendForwardBuf new ContextForwardIO failed, ctxId:%u", ctx->id);
            delete[] pDynBuf;
            return -1;
        }
        ctxIO->ctxForward = ctx;
        ctxIO->bufIO = pDynBuf;
        if (ctx->type == FORWARD_TCP || ctx->type == FORWARD_JDWP || ctx->type == FORWARD_ARK) {
            nRet = Base::SendToStreamEx((uv_stream_t *)&ctx->tcp, pDynBuf, size, nullptr,
                                        (void *)SendCallbackForwardBuf, (void *)ctxIO);
        } else {
            // FORWARD_ABSTRACT, FORWARD_RESERVED, FORWARD_FILESYSTEM,
            nRet = Base::SendToStreamEx((uv_stream_t *)&ctx->pipe, pDynBuf, size, nullptr,
                                        (void *)SendCallbackForwardBuf, (void *)ctxIO);
        }
        if (nRet < 0) {
            WRITE_LOG(LOG_WARN, "SendForwardBuf SendToStreamEx ret:%d, size:%d ctxId:%u type:%d",
                nRet, size, ctx->id, ctx->type);
            delete[] pDynBuf;
            delete ctxIO;
        }
    }
    return nRet;
}

bool HdcForwardBase::CommandForwardCheckResult(HCtxForward ctx, uint8_t *payload)
{
    bool ret = true;
    bool bCheck = static_cast<bool>(payload);
    LogMsg(bCheck ? MSG_OK : MSG_FAIL, "Forwardport result:%s", bCheck ? "OK" : "Failed");
    if (bCheck) {
        string mapInfo = taskInfo->serverOrDaemon ? "1|" : "0|";
        mapInfo += taskCommand;
        ctx->ready = true;
        ServerCommand(CMD_FORWARD_SUCCESS, reinterpret_cast<uint8_t *>(const_cast<char *>(mapInfo.c_str())),
                      mapInfo.size() + 1);
    } else {
        ret = false;
        FreeContext(ctx, 0, false);
    }
    return ret;
}

bool HdcForwardBase::ForwardCommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    if (payloadSize <= DWORD_SERIALIZE_SIZE && command != CMD_FORWARD_FREE_CONTEXT
        && command != CMD_FORWARD_ACTIVE_MASTER) {
        WRITE_LOG(LOG_FATAL, "Illegal payloadSize, shorter than forward command header");
        return false;
    }
    bool ret = true;
    uint8_t *pContent = nullptr;
    int sizeContent = 0;
    uint32_t id = 0;
    HCtxForward ctx = nullptr;
    FilterCommand(payload, &id, &pContent);
    sizeContent = payloadSize - DWORD_SERIALIZE_SIZE;
    if (!(ctx = (HCtxForward)AdminContext(OP_QUERY, id, nullptr))) {
        return true;
    }
    switch (command) {
        case CMD_FORWARD_CHECK_RESULT: {
            ret = CommandForwardCheckResult(ctx, pContent);
            break;
        }
        case CMD_FORWARD_ACTIVE_MASTER: {
            ret = DoForwardBegin(ctx);
            break;
        }
        case CMD_FORWARD_DATA: {
            if (ctx->finish) {
                break;
            }
            if (SendForwardBuf(ctx, pContent, sizeContent) < 0) {
                WRITE_LOG(LOG_WARN, "ForwardCommandDispatch SendForwardBuf rc < 0, ctxid:%u cid:%u sid:%u", id,
                    taskInfo->channelId, taskInfo->sessionId);
                FreeContext(ctx, 0, true);
            }
            break;
        }
        case CMD_FORWARD_FREE_CONTEXT: {
            FreeContext(ctx, 0, false);
            break;
        }
        default:
            ret = false;
            break;
    }
    if (!ret) {
        if (ctx) {
            FreeContext(ctx, 0, true);
        } else {
            WRITE_LOG(LOG_DEBUG, "ctx==nullptr raw free");
            TaskFinish();
        }
    }
    return ret;
}

bool HdcForwardBase::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    if (command != CMD_FORWARD_DATA) {
        WRITE_LOG(LOG_WARN, "CommandDispatch command:%d payloadSize:%d cid:%u sid:%u", command, payloadSize,
            taskInfo->channelId, taskInfo->sessionId);
    }
    bool ret = true;
    string sError;
    // prepare
    if (command == CMD_FORWARD_INIT) {
        string strCommand(reinterpret_cast<char *>(payload), payloadSize);
        if (!BeginForward(strCommand, sError)) {
            ret = false;
            goto Finish;
        }
        return true;
    } else if (command == CMD_FORWARD_CHECK) {
        // Detect remote if it's reachable
        if (!SlaveConnect(payload, payloadSize, true, sError)) {
            ret = false;
            goto Finish;
        }
        return true;
    } else if (command == CMD_FORWARD_ACTIVE_SLAVE) {
        // slave connect target port when activating
        if (!SlaveConnect(payload, payloadSize, false, sError)) {
            ret = false;
            goto Finish;
        }
        return true;
    }
    if (!ForwardCommandDispatch(command, payload, payloadSize)) {
        ret = false;
        goto Finish;
    }
Finish:
    if (!ret) {
        if (!sError.size()) {
            LogMsg(MSG_FAIL, "Forward parament failed");
        } else {
            LogMsg(MSG_FAIL, const_cast<char *>(sError.c_str()));
            WRITE_LOG(LOG_WARN, const_cast<char *>(sError.c_str()));
        }
    }
    return ret;
}
}  // namespace Hdc
