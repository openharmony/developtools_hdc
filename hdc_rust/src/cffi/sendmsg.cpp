/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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
#include "securec.h"
#include "sendmsg.h"
#include "log.h"
#include <sys/socket.h>
#include <cstring>
#include <alloca.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>

namespace Hdc {

extern "C" int SendMsg(int socket_fd, int fd, char* data, int size)
{
    constexpr int memcpyError = -2;
    constexpr int cmsgNullptrError = -5;
    struct iovec iov;
    iov.iov_base = data;
    iov.iov_len = size;
    struct msghdr msg;
    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    int len = CMSG_SPACE(static_cast<unsigned int>(sizeof(int)));
    char ctlBuf[len];
    msg.msg_control = ctlBuf;
    msg.msg_controllen = sizeof(ctlBuf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == nullptr) {
        WRITE_LOG(LOG_WARN, "SendFdToApp cmsg is nullptr\n");
        return cmsgNullptrError;
    }
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    int result = -1;
    if (memcpy_s(CMSG_DATA(cmsg), sizeof(int), &fd, sizeof(int)) != EOK) {
        WRITE_LOG(LOG_WARN, "SendFdToApp memcpy_s error:%d\n", errno);
        return memcpyError;
    }
    if ((result = sendmsg(socket_fd, &msg, 0)) < 0) {
        WRITE_LOG(LOG_WARN, "SendFdToApp sendmsg errno:%d, result:%d\n", errno, result);
        return result;
    }
    WRITE_LOG(LOG_INFO, "send msg ok\n");
    return result;
}
}