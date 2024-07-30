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
#include "parameter.h"
#include <sys/epoll.h>
#include <unistd.h>

namespace Hdc {

HdcJdwpSimulator::HdcJdwpSimulator(const std::string processName, const std::string pkgName, bool isDebug, Callback cb)
{
    processName_ = processName;
    pkgName_ = pkgName;
    isDebug_ = isDebug;
    cb_ = cb;
    cfd_ = -1;
    disconnectFlag_ = false;
    notified_ = false;
    AddWatchHdcdJdwp();
}

void HdcJdwpSimulator::Disconnect()
{
    disconnectFlag_ = true;
    cv_.notify_one();
    if (cfd_ > -1) {
        shutdown(cfd_, SHUT_RDWR);
        close(cfd_);
        cfd_ = -1;
    }
}

HdcJdwpSimulator::~HdcJdwpSimulator()
{
    Disconnect();
    DelWatchHdcdJdwp();
}

bool HdcJdwpSimulator::SendBuf(const uint8_t *buf, const int bufLen)
{
    ssize_t rc = write(cfd_, buf, bufLen);
    if (rc < 0) {
        HILOG_FATAL(LOG_CORE, "SendBuf failed errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool HdcJdwpSimulator::Connect2Jdwp()
{
    const char jdwp[] = { '\0', 'o', 'h', 'j', 'p', 'i', 'd', '-', 'c', 'o', 'n', 't', 'r', 'o', 'l', 0 };
    struct sockaddr_un caddr;
    if (memset_s(&caddr, sizeof(caddr), 0, sizeof(caddr)) != EOK) {
        HILOG_FATAL(LOG_CORE, "memset_s failed");
        return false;
    }
    caddr.sun_family = AF_UNIX;
    for (size_t i = 0; i < sizeof(jdwp); i++) {
        caddr.sun_path[i] = jdwp[i];
    }
    cfd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (cfd_ < 0) {
        HILOG_FATAL(LOG_CORE, "socket failed errno:%{public}d", errno);
        return false;
    }
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(cfd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    size_t caddrLen = sizeof(caddr.sun_family) + sizeof(jdwp) - 1;
    int retry = 3;
    int rc = 0;
    while (retry-- > 0) {
        rc = connect(cfd_, reinterpret_cast<struct sockaddr *>(&caddr), caddrLen);
        if (rc == 0) {
            HILOG_INFO(LOG_CORE, "connect success cfd:%{public}d", cfd_);
            break;
        }
        constexpr int to = 3;
        sleep(to);
    }
    if (rc != 0) {
        HILOG_INFO(LOG_CORE, "connect failed cfd_:%{public}d", cfd_);
        close(cfd_);
        cfd_ = -1;
        return false;
    }
    return true;
}

bool HdcJdwpSimulator::Send2Jdwp()
{
    uint32_t pidCurr = static_cast<uint32_t>(getprocpid());
#ifdef JS_JDWP_CONNECT
    std::string processName = processName_;
    std::string pkgName = pkgName_;
    bool isDebug = isDebug_;
    std::string pp = pkgName;
    if (!processName.empty()) {
        pp += "/" + processName;
    }
    uint32_t ppSize = pp.size() + sizeof(JsMsgHeader);
    uint8_t* info = new (std::nothrow) uint8_t[ppSize]();
    if (info == nullptr) {
        HILOG_FATAL(LOG_CORE, "Send2Jdwp new info fail.");
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
    HILOG_INFO(LOG_CORE,
        "Send2Jdwp send pid:%{public}d, pp:%{public}s, isDebug:%{public}d, msglen:%{public}d",
        jsMsg->pid, pp.c_str(), isDebug, jsMsg->msgLen);
    bool ret = true;
    if (memcpy_s(info + sizeof(JsMsgHeader), pp.size(), &pp[0], pp.size()) != EOK) {
        HILOG_FATAL(LOG_CORE, "Send2Jdwp memcpy_s fail :%{public}s.", pp.c_str());
        ret = false;
    } else {
        ret = SendBuf(static_cast<uint8_t*>(info), ppSize);
    }
    delete[] info;
    return ret;
#endif
    return false;
}

void HdcJdwpSimulator::ReadFromJdwp()
{
    constexpr size_t size = 256;
    constexpr long sec = 5;
    constexpr long ms = 1000;
    uint8_t buf[size] = { 0 };
    constexpr int maxevents = 1;
    struct epoll_event ev;
    struct epoll_event evs[maxevents];
    int efd = epoll_create(maxevents);
    if (efd == -1) {
        HILOG_FATAL(LOG_CORE, "Read epoll_create error:%{public}d", errno);
        return;
    }
    ev.data.fd = cfd_;
    ev.events = EPOLLIN ;
    int rc = epoll_ctl(efd, EPOLL_CTL_ADD, cfd_, &ev);
    if (rc == -1) {
        HILOG_FATAL(LOG_CORE, "Read epoll_ctl add cfd:%{public}d error:%{public}d",
            cfd_, errno);
        close(efd);
        return;
    }
    while (!disconnectFlag_) {
        ssize_t cnt = 0;
        ssize_t minlen = sizeof(int32_t);
        rc = epoll_wait(efd, evs, maxevents, sec * ms);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            HILOG_FATAL(LOG_CORE, "Read epoll_wait cfd:%{public}d error:%{public}d",
                cfd_, errno);
            break;
        } else if (rc == 0) {
            continue;
        }
        int rfd = evs[0].data.fd;
        if (memset_s(buf, sizeof(buf), 0, sizeof(buf)) != EOK) {
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
        cnt = recvmsg(rfd, &msg, 0);
        if (cnt < 0) {
            HILOG_FATAL(LOG_CORE, "Read recvmsg rfd:%{public}d errno:%{public}d", rfd, errno);
            break;
        } else if (cnt == 0) {
            HILOG_WARN(LOG_CORE, "Read recvmsg socket peer closed rfd:%{public}d", rfd);
            break;
        } else if (cnt < minlen) {
            HILOG_WARN(LOG_CORE, "Read recvmsg cnt:%{public}zd rfd:%{public}d", cnt, rfd);
            continue;
        }
        int32_t fd = *reinterpret_cast<int32_t *>(buf);
        std::string str(reinterpret_cast<char *>(buf + sizeof(int32_t)), cnt - sizeof(int32_t));
        HILOG_INFO(LOG_CORE, "Read fd:%{public}d str:%{public}s", fd, str.c_str());
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg == nullptr) {
            HILOG_FATAL(LOG_CORE, "Read cmsg is nullptr");
            continue;
        }
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS ||
            cmsg->cmsg_len != CMSG_LEN(sizeof(int))) {
            HILOG_INFO(LOG_CORE, "Read level:%{public}d type:%{public}d len:%{public}d",
                cmsg->cmsg_level, cmsg->cmsg_type, cmsg->cmsg_len);
            continue;
        }
        int newfd = *(reinterpret_cast<int *>(CMSG_DATA(cmsg)));
        HILOG_INFO(LOG_CORE, "Read fd:%{public}d newfd:%{public}d str:%{public}s",
            fd, newfd, str.c_str());
        if (cb_) {
            cb_(newfd, str);
        }
    }
    rc = epoll_ctl(efd, EPOLL_CTL_DEL, cfd_, nullptr);
    if (rc == -1) {
        HILOG_WARN(LOG_CORE, "Read epoll_ctl del cfd:%{public}d error:%{public}d", cfd_, errno);
    }
    close(cfd_);
    cfd_ = -1;
    close(efd);
}

bool HdcJdwpSimulator::Connect()
{
    while (!disconnectFlag_) {
        bool b = Connect2Jdwp();
        if (!b) {
            HILOG_INFO(LOG_CORE, "Connect2Jdwp failed cfd:%{public}d", cfd_);
            WaitForJdwp();
            continue;
        }
        b = Send2Jdwp();
        if (!b) {
            HILOG_WARN(LOG_CORE, "Send2Jdwp failed cfd:%{public}d", cfd_);
            continue;
        }
        ReadFromJdwp();
    }
    return true;
}

void HdcJdwpSimulator::WaitForJdwp()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() -> bool { return this->notified_ || this->disconnectFlag_; });
    }
    notified_ = false;
}

void HdcJdwpSimulator::AddWatchHdcdJdwp()
{
    auto eventCallback = [](const char *key, const char *value, void *context) {
        auto that = reinterpret_cast<HdcJdwpSimulator *>(context);
        if (strncmp(key, PERSIST_HDC_JDWP, strlen(PERSIST_HDC_JDWP)) != 0) {
            return;
        }
        if (strncmp(value, "1", strlen("1")) != 0) {
            return;
        }
        that->notified_ = true;
        that->cv_.notify_one();
    };
    int rc = WatchParameter(PERSIST_HDC_JDWP, eventCallback, this);
    HILOG_INFO(LOG_CORE, "AddWatchHdcdJdwp rc:%{public}d", rc);
}

void HdcJdwpSimulator::DelWatchHdcdJdwp()
{
    int rc = RemoveParameterWatcher(PERSIST_HDC_JDWP, nullptr, nullptr);
    HILOG_INFO(LOG_CORE, "DelWatchHdcdJdwp rc:%{public}d", rc);
}
} // namespace Hdc
