/*
 * Copyright (C) 2021-2022 Huawei Device Co., Ltd.
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
 *
 */
#include "hdc_jdwp.h"

#include <unistd.h>

namespace Hdc {

HdcJdwpSimulator::HdcJdwpSimulator(const std::string processName, const std::string pkgName, bool isDebug, Callback cb)
{
    processName_ = processName;
    pkgName_ = pkgName;
    isDebug_ = isDebug;
    cb_ = cb;
    cfd_ = -1;
    ctxPoint_ = static_cast<HCtxJdwpSimulator>(MallocContext());
    disconnectFlag_ = false;
    startOnce_ = true;
}

void HdcJdwpSimulator::Disconnect()
{
    disconnectFlag_ = true;
    if (ctxPoint_ != nullptr && ctxPoint_->cfd > -1) {
        shutdown(ctxPoint_->cfd, SHUT_RDWR);
        close(ctxPoint_->cfd);
        ctxPoint_->cfd = -1;
    }
    if (readThread_.joinable()) {
        readThread_.join();
    }
}

HdcJdwpSimulator::~HdcJdwpSimulator()
{
    disconnectFlag_ = true;
    if (ctxPoint_ != nullptr && ctxPoint_->cfd > -1) {
        shutdown(ctxPoint_->cfd, SHUT_RDWR);
        close(ctxPoint_->cfd);
        ctxPoint_->cfd = -1;
    }
    if (readThread_.joinable()) {
        readThread_.join();
    }
    if (ctxPoint_ != nullptr) {
        delete ctxPoint_;
        ctxPoint_ = nullptr;
    }
}

bool HdcJdwpSimulator::SendToJpid(int fd, const uint8_t *buf, const int bufLen)
{
    OHOS::HiviewDFX::HiLog::Info(LOG_LABEL, "SendToJpid: %{public}s, %{public}d", buf, bufLen);
    ssize_t rc = write(fd, buf, bufLen);
    if (rc < 0) {
        OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "SendToJpid failed errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool HdcJdwpSimulator::ConnectJpid(HdcJdwpSimulator *param)
{
    uint32_t pidCurr = static_cast<uint32_t>(getpid());
    HdcJdwpSimulator *thisClass = param;
#ifdef JS_JDWP_CONNECT
    string processName = thisClass->processName_;
    string pkgName = thisClass->pkgName_;
    bool isDebug = thisClass->isDebug_;
    string pp = pkgName;
    if (!processName.empty()) {
        pp += "/" + processName;
    }
    uint32_t ppSize = pp.size() + sizeof(JsMsgHeader);
    uint8_t* info = new (std::nothrow) uint8_t[ppSize]();
    if (info == nullptr) {
        OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "ConnectJpid new info fail.");
        return false;
    }
    if (memset_s(info, ppSize, 0, ppSize) != EOK) {
        delete[] info;
        info = nullptr;
        return false;
    }
    JsMsgHeader *jsMsg = reinterpret_cast<JsMsgHeader *>(info);
    jsMsg->msgLen = ppSize;
    jsMsg->pid = pidCurr;
    jsMsg->isDebug = isDebug;
    OHOS::HiviewDFX::HiLog::Info(LOG_LABEL,
        "ConnectJpid send pid:%{public}d, pp:%{public}s, isDebug:%{public}d, msglen:%{public}d",
        jsMsg->pid, pp.c_str(), isDebug, jsMsg->msgLen);
    bool ret = true;
    if (memcpy_s(info + sizeof(JsMsgHeader), pp.size(), &pp[0], pp.size()) != EOK) {
        OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "ConnectJpid memcpy_s fail :%{public}s.", pp.c_str());
        ret = false;
    } else {
        OHOS::HiviewDFX::HiLog::Info(LOG_LABEL, "ConnectJpid send JS msg:%{public}s", info);
        ret = SendToJpid(thisClass->ctxPoint_->cfd, static_cast<uint8_t*>(info), ppSize);
    }
    delete[] info;
    return ret;
#endif
    return false;
}

void *HdcJdwpSimulator::MallocContext()
{
    HCtxJdwpSimulator ctx = nullptr;
    if ((ctx = new (std::nothrow) ContextJdwpSimulator()) == nullptr) {
        return nullptr;
    }
    ctx->thisClass = this;
    ctx->cfd = -1;
    return ctx;
}

bool HdcJdwpSimulator::Connect()
{
    const char jdwp[] = { '\0', 'o', 'h', 'j', 'p', 'i', 'd', '-', 'c', 'o', 'n', 't', 'r', 'o', 'l', 0 };
    if (ctxPoint_ == nullptr) {
        OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "MallocContext failed");
        return false;
    }
    struct sockaddr_un caddr;
    if (memset_s(&caddr, sizeof(caddr), 0, sizeof(caddr)) != EOK) {
        OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "memset_s failed");
        return false;
    }
    caddr.sun_family = AF_UNIX;
    for (size_t i = 0; i < sizeof(jdwp); i++) {
        caddr.sun_path[i] = jdwp[i];
    }
    cfd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (cfd_ < 0) {
        OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "socket failed errno:%{public}d", errno);
        return false;
    }
    ctxPoint_->cfd = cfd_;

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(cfd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    size_t caddrLen = sizeof(caddr.sun_family) + sizeof(jdwp) - 1;
    int rc = connect(cfd_, reinterpret_cast<struct sockaddr *>(&caddr), caddrLen);
    if (rc != 0) {
        OHOS::HiviewDFX::HiLog::Info(LOG_LABEL, "connect failed errno:%{public}d", errno);
        close(cfd_);
        cfd_ = -1;
        return false;
    }
    if (ConnectJpid(this)) {
        if (startOnce_) {
            startOnce_ = false;
            ReadStart();
        }
    }
    return true;
}

void HdcJdwpSimulator::ReadStart()
{
    readThread_ = std::thread(ReadWork, this);
}

void HdcJdwpSimulator::ReadWork(HdcJdwpSimulator *param)
{
    HdcJdwpSimulator *jdwp = param;
    jdwp->Read();
}

void HdcJdwpSimulator::Read()
{
    constexpr size_t size = 256;
    constexpr long sec = 5;
    uint8_t buf[size] = { 0 };
    while (!disconnectFlag_ && cfd_ > -1) {
        ssize_t cnt = 0;
        ssize_t minlen = sizeof(int32_t);
        fd_set rset;
        struct timeval timeout;
        timeout.tv_sec = sec;
        timeout.tv_usec = 0;
        FD_ZERO(&rset);
        FD_SET(cfd_, &rset);
        int rc = select(cfd_ + 1, &rset, nullptr, nullptr, &timeout);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "Read select fd:%{public}d error:%{public}d", cfd_, errno);
            break;
        } else if (rc == 0) {
            continue;
        }
        if (memset_s(buf, size, 0, size) != EOK) {
            continue;
        }
        struct iovec iov;
        iov.iov_base = buf;
        iov.iov_len = size - 1;
        struct msghdr msg;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        int len = CMSG_SPACE(static_cast<unsigned int>(sizeof(int)));
        char ctlBuf[len];
        msg.msg_controllen = sizeof(ctlBuf);
        msg.msg_control = ctlBuf;
        cnt = recvmsg(cfd_, &msg, 0);
        if (cnt < 0) {
            OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "Read recvmsg cfd:%{public}d errno:%{public}d", cfd_, errno);
            break;
        } else if (cnt == 0) {
            OHOS::HiviewDFX::HiLog::Warn(LOG_LABEL, "Read recvmsg socket peer closed cfd:%{public}d", cfd_);
            close(cfd_);
            cfd_ = -1;
            Reconnect();
            continue;
        } else if (cnt < minlen) {
            OHOS::HiviewDFX::HiLog::Warn(LOG_LABEL, "Read recvmsg cnt:%{public}d cfd:%{public}d", cnt, cfd_);
            continue;
        }
        int32_t fd = *reinterpret_cast<int32_t *>(buf);
        std::string str(reinterpret_cast<char *>(buf + sizeof(int32_t)), cnt - sizeof(int32_t));
        OHOS::HiviewDFX::HiLog::Info(LOG_LABEL, "Read fd:%{public}d str:%{public}s", fd, str.c_str());
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg == nullptr) {
            OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "Read cmsg is nullptr");
            continue;
        }
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS ||
            cmsg->cmsg_len != CMSG_LEN(sizeof(int))) {
            OHOS::HiviewDFX::HiLog::Info(LOG_LABEL, "Read level:%{public}d type:%{public}d len:%{public}d",
                cmsg->cmsg_level, cmsg->cmsg_type, cmsg->cmsg_len);
            continue;
        }
        int newfd = *(reinterpret_cast<int *>(CMSG_DATA(cmsg)));
        OHOS::HiviewDFX::HiLog::Info(LOG_LABEL, "Read fd:%{public}d newfd:%{public}d str:%{public}s",
            fd, newfd, str.c_str());
        if (cb_) {
            cb_(newfd, str);
        }
    }
}

void HdcJdwpSimulator::Reconnect()
{
    constexpr int timeout = 3;
    int retry = 5;
    // wait for hdcd restart
    sleep(timeout);
    while (!disconnectFlag_ && retry > 0) {
        bool c = Connect();
        if (c) {
            OHOS::HiviewDFX::HiLog::Info(LOG_LABEL, "Reconnect success cfd:%{public}d", cfd_);
            break;
        }
        OHOS::HiviewDFX::HiLog::Warn(LOG_LABEL, "Reconnect cfd:%{public}d retry:%{public}d", cfd_, retry--);
        sleep(timeout);
    }
}
} // namespace Hdc
