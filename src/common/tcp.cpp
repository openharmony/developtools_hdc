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
#include "tcp.h"

namespace Hdc {
HdcTCPBase::HdcTCPBase(const bool serverOrDaemonIn, void *ptrMainBase)
{
    // Calling the initialization
    InitialChildClass(serverOrDaemonIn, ptrMainBase);
}

HdcTCPBase::~HdcTCPBase()
{
}

// Subclasses must be explicitly called
void HdcTCPBase::InitialChildClass(const bool serverOrDaemonIn, void *ptrMainBase)
{
    serverOrDaemon = serverOrDaemonIn;
    clsMainBase = ptrMainBase;
}

void HdcTCPBase::RecvUDP(uv_udp_t *handle, ssize_t nread, const uv_buf_t *rcvbuf, const struct sockaddr *addr,
                         unsigned /* flags */)
{
    while (true) {
        HdcTCPBase *thisClass = (HdcTCPBase *)handle->data;
        if (nread <= 0) {
            // ==0 finish;<0 error
            break;
        }
        CALLSTAT_GUARD(((HdcSessionBase *)(thisClass->clsMainBase))->loopMainStatus,
                       handle->loop, "HdcTCPBase::RecvUDP");
        WRITE_LOG(LOG_DEBUG, "RecvUDP %s", rcvbuf->base);
        if (strncmp(rcvbuf->base, HANDSHAKE_MESSAGE.c_str(), HANDSHAKE_MESSAGE.size())) {
            break;
        }
        thisClass->RecvUDPEntry(addr, handle, rcvbuf);
        break;
    }
    delete[] (uint8_t*)rcvbuf->base;
}

void HdcTCPBase::AllocStreamUDP(uv_handle_t* /* handle */, size_t /* sizeWanted */, uv_buf_t *buf)
{
    size_t bufLen = BUF_SIZE_DEFAULT;
    char *pRecvBuf = reinterpret_cast<char *>(new uint8_t[bufLen]());
    if (!pRecvBuf) {
        return;
    }
    buf->base = pRecvBuf;
    buf->len = bufLen;
}

void HdcTCPBase::SendUDPFinish(uv_udp_send_t *req, int /* status */)
{
    delete req;
}

void HdcTCPBase::ReadStream(uv_stream_t *tcp, ssize_t nread, const uv_buf_t *buf)
{
    HSession hSession = (HSession)tcp->data;
    HdcTCPBase *thisClass = (HdcTCPBase *)hSession->classModule;
    HdcSessionBase *hSessionBase = (HdcSessionBase *)thisClass->clsMainBase;
    CALLSTAT_GUARD(hSession->childLoopStatus, tcp->loop, "HdcTCPBase::ReadStream");
    bool ret = false;
    while (true) {
        if (nread < 0) {
            break;
        }
#ifdef HDC_SUPPORT_ENCRYPT_TCP
        if (hSessionBase->FetchIOBuf(hSession, hSession->ioBuf, nread, hSession->sslHandshake) < 0) {
            WRITE_LOG(LOG_FATAL, "ReadStream FetchIOBuf error nread:%zd, sid:%u", nread, hSession->sessionId);
            break;
        }
#else
        if (hSessionBase->FetchIOBuf(hSession, hSession->ioBuf, nread) < 0) {
            WRITE_LOG(LOG_FATAL, "ReadStream FetchIOBuf error nread:%zd, sid:%u", nread, hSession->sessionId);
            break;
        }
#endif
        ret = true;
        break;
    }
    if (!ret) {
        char buffer[BUF_SIZE_DEFAULT] = { 0 };
        if (nread < 0) {
            uv_strerror_r(static_cast<int>(nread), buffer, BUF_SIZE_DEFAULT);
            WRITE_LOG(LOG_INFO, "HdcTCPBase::ReadStream < 0 %s sid:%u", buffer, hSession->sessionId);
        }
#ifdef HDC_HOST
        hSession->isRunningOk = false;
        hSession->faultInfo = (nread < 0) ? buffer : "package parse error";
#endif
        // The first time is closed first, prevent the write function from continuing to write
        Base::TryCloseHandle(reinterpret_cast<uv_handle_t *>(tcp));
        hSessionBase->FreeSession(hSession->sessionId);
    }
}

bool HdcTCPBase::WriteUvSslFd(uv_tcp_t *tcp, uint8_t *buf, int size, int &cnt)
{
    if (!tcp || !buf || size < 0 ||cnt < 0) {
        WRITE_LOG(LOG_FATAL, "WriteUvSslFd error, input parameter abnormal.");
        return false;
    }
#ifdef HDC_SUPPORT_ENCRYPT_TCP
    HSession hSession = reinterpret_cast<HSession>(tcp->data);
    if (!hSession) {
        WRITE_LOG(LOG_FATAL, "WriteUvSslFd error, hSession is null.");
        return false;
    }

    if (!hSession->sslHandshake) { // plaintext tcp transfer.
        return true;
    }

    HdcSSLBase *hssl = static_cast<HdcSSLBase *>(hSession->classSSL);
    if (!hssl) {
        WRITE_LOG(LOG_FATAL, "WriteUvSslFd error, hssl is null.");
        return false;
    }

    cnt = hssl->Encrypt(size, buf);
    if (cnt < 0) {
        WRITE_LOG(LOG_FATAL, "WriteSSL error, cnt:%d", cnt);
        return false;
    }
#endif
    return true;
}

int HdcTCPBase::WriteUvTcpFd(uv_tcp_t *tcp, uint8_t *buf, int size)
{
    std::lock_guard<std::mutex> lock(writeTCPMutex);
    uint8_t *data = buf;
    int cnt = size;
    if (!WriteUvSslFd(tcp, buf, size, cnt)) {
        delete[] buf;
        return ERR_GENERIC;
    }
    uv_os_fd_t uvfd;
    uv_fileno(reinterpret_cast<uv_handle_t*>(tcp), &uvfd);
#ifdef _WIN32
    int fd = (uv_os_sock_t)uvfd;
#else
    int fd = reinterpret_cast<int>(uvfd);
#endif
    constexpr int intrmax = 60000;
    int intrcnt = 0;
    while (cnt > 0) {
#ifdef HDC_EMULATOR
        int rc = write(fd, reinterpret_cast<const char*>(data), cnt);
#else
        int rc = send(fd, reinterpret_cast<const char*>(data), cnt, 0);
#endif
        if (rc < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEINTR || err == WSAEWOULDBLOCK) {
#else
            int err = errno;
            if (err == EINTR || err == EAGAIN) {
#endif
                if (++intrcnt > intrmax) {
                    WRITE_LOG(LOG_WARN, "WriteUvTcpFd fd:%d send interrupt err:%d", fd, err);
                    intrcnt = 0;
                }
                std::this_thread::yield();
                continue;
            } else {
                WRITE_LOG(LOG_FATAL, "WriteUvTcpFd fd:%d send rc:%d err:%d", fd, rc, err);
                cnt = ERR_GENERIC;
                break;
            }
        }
        data += rc;
        cnt -= rc;
    }
    delete[] buf;
    return cnt == 0 ? size : cnt;
}
}  // namespace Hdc
