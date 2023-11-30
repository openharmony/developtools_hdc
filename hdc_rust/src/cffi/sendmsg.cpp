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
#include "sendmsg.h"

#include <sys/socket.h>
#include <string.h>
#include <alloca.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

namespace Hdc {

extern "C" int SendMsg_(int socket_fd, int fd, char* data, int size) {
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
        printf("SendFdToApp cmsg is nullptr\n");
        return -5;
    }
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    int result = -1;
    if (memcpy(CMSG_DATA(cmsg), &fd, sizeof(int)) == nullptr) {
        printf("SendFdToApp memcpy error:%d\n", errno);
        return -2;
    }
    if ((result = sendmsg(socket_fd, &msg, 0)) < 0) {
        printf("SendFdToApp sendmsg errno:%d, result:%d\n", errno, result);
        return result;
    }
    printf("send msg ok\n");
    return result;
}
}