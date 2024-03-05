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
#include "daemon_bridge.h"
#include <cstdlib>
#include "arpa/inet.h"
#include "netinet/in.h"
#include "new"
#include "sys/socket.h"
#include "system_depend.h"
#include "unistd.h"
#include "common.h"
#include "session.h"
#include "uv-common.h"

namespace Hdc {
    HdcDaemonBridge::HdcDaemonBridge(const bool serverOrDaemonIn, void *ptrMainBase)
        : HdcTCPBase(serverOrDaemonIn, ptrMainBase)
    {
        // If the listening value for the property setting is obtained, it will be 0 randomly assigned.
        string strBridgePort;
        SystemDepend::GetDevItem("persist.hdc.port", strBridgePort);
        bridgeListenPort = atoi(strBridgePort.c_str());
        if (bridgeListenPort <= 0) {
            bridgeListenPort = 0;
        }
#ifdef HDC_DEBUG
        const uint16_t DEBUG_TCP_PORT = 10178;
        bridgeListenPort = DEBUG_TCP_PORT;
#endif
    }

    HdcDaemonBridge::~HdcDaemonBridge()
    {
    }

    void HdcDaemonBridge::Stop()
    {
        Base::TryCloseHandle((const uv_handle_t *)&servPipe);
        WRITE_LOG(LOG_DEBUG, "~HdcDaemonBridge");
    }

    void HdcDaemonBridge::AllocBufferCallback(uv_handle_t *handle, size_t sizeSuggested, uv_buf_t *buf)
    {
        buf->base = (char *)new int[1];
        buf->len = SOCKET_FD_LEN;
    }

    void HdcDaemonBridge::AcceptClient(uv_stream_t *uvpipe, ssize_t nread, const uv_buf_t *buf)
    {
        WRITE_LOG(LOG_DEBUG, "Accept New Client");
        uv_loop_t *ptrLoop = uvpipe->loop;
        uv_tcp_t *pServBridge = (uv_tcp_t *)uvpipe;
        HdcDaemonBridge *thisClass = (HdcDaemonBridge *)pServBridge->data;
        HdcSessionBase *ptrConnect = reinterpret_cast<HdcSessionBase *>(thisClass->clsMainBase);
        const uint16_t maxWaitTime = UV_DEFAULT_INTERVAL;
        int newPort = *(int *)buf->base;
        int newClientFd;
        int ret = 0;
        if (nread != SOCKET_FD_LEN) {
            WRITE_LOG(LOG_FATAL, "AcceptClient err nread %d", nread);
            return;
        }
        auto ctrl = ptrConnect->BuildCtrlString(SP_START_SESSION, 0, nullptr, 0);
        HSession hSession = ptrConnect->MallocSession(false, CONN_TCP, thisClass);
        if (!hSession) {
            goto freeBuf;
        }

        newClientFd = open("/dev/express_bridge", O_RDWR | O_NONBLOCK);
        if (newClientFd < 0) {
            WRITE_LOG(LOG_FATAL, "Unable to open new bridge connection err %d", errno);
            goto freeSession;
        }
        ret = ioctl(newClientFd, IOC_CONNECT, static_cast<unsigned long>(newPort));
        if (ret < 0) {
            WRITE_LOG(LOG_FATAL, "Unable to ioctl new bridge err %d", errno);
            goto freeFd;
        }
        hSession->fdChildWorkTCP = (uv_os_sock_t)newClientFd;
        Base::StartWorkThread(ptrLoop, ptrConnect->SessionWorkThread, Base::FinishWorkThread, hSession);
        // wait for thread up
        while (hSession->childLoop.active_handles == 0) {
            WRITE_LOG(LOG_DEBUG, "wait for thread up");
            usleep(maxWaitTime);
        }
        Base::SendToPollFd(hSession->ctrlFd[STREAM_MAIN], ctrl.data(), ctrl.size());
        goto freeBuf;

    freeFd:
        close(newClientFd);
    freeSession:
        ptrConnect->FreeSession(hSession->sessionId);
    freeBuf:
        delete[] buf->base;
        return;
    }

    // Set the daemon-side TCP listening
    int HdcDaemonBridge::SetBridgeListen()
    {
        // tcp listen
        HdcSessionBase *ptrConnect = (HdcSessionBase *)clsMainBase;
        servPipe.data = this;
        int bridgeFd = open("/dev/express_bridge", O_RDWR | O_NONBLOCK);
        if (bridgeFd <= 0) {
            WRITE_LOG(LOG_FATAL, "SetBridgeListen open failed");
            return ERR_FILE_OPEN;
        }
        int ret = ioctl(bridgeFd, IOC_BIND, (unsigned long)bridgeListenPort);
        if (ret < 0) {
            WRITE_LOG(LOG_FATAL, "SetBridgeListen IOC_BIND failed");
            return ERR_API_FAIL;
        }

        uv_pipe_init(&ptrConnect->loopMain, &servPipe, 0);
        uv_pipe_open(&servPipe, (uv_file)bridgeFd);
        uv_read_start((uv_stream_t *)&servPipe, HdcDaemonBridge::AllocBufferCallback, HdcDaemonBridge::AcceptClient);

        return RET_SUCCESS;
    }

    int HdcDaemonBridge::Initial()
    {
        WRITE_LOG(LOG_DEBUG, "HdcDaemonBridge init");
        if (SetBridgeListen() != RET_SUCCESS) {
            WRITE_LOG(LOG_FATAL, "Bridge listen failed");
            return ERR_GENERIC;
        }
#ifndef UNIT_TEST
        WRITE_LOG(LOG_INFO, "Bridge listen on port:[%d]", bridgeListenPort);
#endif
        return RET_SUCCESS;
    }
} // namespace Hdc
