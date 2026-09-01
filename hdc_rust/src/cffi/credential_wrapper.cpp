/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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
#include "credential_wrapper.h"
#include <securec.h>
#include <cstring>

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#else
#include <winsock2.h>
#include <afunix.h>
#endif

namespace {
constexpr uint8_t CRED_VERSION = 1;
constexpr int CRED_LEN_OFFSET = 2;
constexpr int CRED_HEAD_SIZE = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t);

bool ValidateParseArgs(const uint8_t *data, int dataLen,
                       const CredentialParseOut *out)
{
    if (data == nullptr || out == nullptr) {
        return false;
    }
    if (out->method == nullptr || out->body == nullptr || out->bodyLen == nullptr) {
        return false;
    }
    if (dataLen < CRED_HEAD_SIZE) {
        return false;
    }
    return true;
}

bool ValidateBodyLen(int dataLen, int maxBody, uint16_t len)
{
    if (dataLen < CRED_HEAD_SIZE + len) {
        return false;
    }
    if (maxBody < len) {
        return false;
    }
    return true;
}
}

extern "C" {
int CredentialBuild(uint8_t method, const uint8_t *body, uint16_t bodyLen,
                    uint8_t *out, int maxOut)
{
    if (body == nullptr || out == nullptr) {
        return -1;
    }
    int total = CRED_HEAD_SIZE + bodyLen;
    if (maxOut < total) {
        return -1;
    }
    out[0] = CRED_VERSION;
    out[1] = method;
    uint16_t len = bodyLen;
    if (memcpy_s(out + CRED_LEN_OFFSET, maxOut - CRED_LEN_OFFSET,
                 &len, sizeof(uint16_t)) != EOK) {
        return -1;
    }
    if (memcpy_s(out + CRED_HEAD_SIZE, maxOut - CRED_HEAD_SIZE,
                 body, bodyLen) != EOK) {
        return -1;
    }
    return total;
}

int CredentialParse(const uint8_t *data, int dataLen,
                    CredentialParseOut *out)
{
    if (!ValidateParseArgs(data, dataLen, out)) {
        return -1;
    }
    uint16_t len = 0;
    if (memcpy_s(&len, sizeof(uint16_t), data + CRED_LEN_OFFSET,
                 sizeof(uint16_t)) != EOK) {
        return -1;
    }
    if (!ValidateBodyLen(dataLen, out->maxBody, len)) {
        return -1;
    }
    *out->method = data[1];
    *out->bodyLen = len;
    if (memcpy_s(out->body, out->maxBody, data + CRED_HEAD_SIZE, len) != EOK) {
        return -1;
    }
    return CRED_HEAD_SIZE + len;
}

int CredentialSendBySocket(const char *path, const uint8_t *data, int len)
{
    if (path == nullptr || data == nullptr) {
        return -1;
    }
#ifndef _WIN32
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    if (strncpy_s(addr.sun_path, sizeof(addr.sun_path), path,
                   sizeof(addr.sun_path) - 1) != EOK) {
        close(fd);
        return -1;
    }
    if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    int sent = send(fd, data, len, 0);
    close(fd);
    return sent;
#else
    return -1;
#endif
}

int CredentialRecvBySocket(const char *path, uint8_t *buf, int maxLen)
{
    if (path == nullptr || buf == nullptr) {
        return -1;
    }
#ifndef _WIN32
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    if (strncpy_s(addr.sun_path, sizeof(addr.sun_path), path,
                   sizeof(addr.sun_path) - 1) != EOK) {
        close(fd);
        return -1;
    }
    if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    int recvd = recv(fd, buf, maxLen, 0);
    close(fd);
    return recvd;
#else
    return -1;
#endif
}
}
