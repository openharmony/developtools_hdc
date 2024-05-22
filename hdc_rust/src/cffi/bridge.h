/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

#ifndef HDC_RUST_BRIDGE_H
#define HDC_RUST_BRIDGE_H

#include "arpa/inet.h"
#include "netinet/in.h"
#include "sys/socket.h"
#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include "ffi_utils.h"

namespace Hdc {
#define IOC_MAGIC 0xE6
#define IOC_BIND _IOW(IOC_MAGIC, 1, int)
#define IOC_CONNECT _IOW(IOC_MAGIC, 2, int)

struct PersistBuffer {
    char* ptr;
    uint64_t size;
};

class HdcBridge {
public:
    HdcBridge();
    ~HdcBridge();
    int StartListen();
    int HandleClient(int socketFd);
    int ReadPipeFd(int fd, char* buf, int size);
    PersistBuffer ReadClient(int fd, int size);
    int WriteClient(int fd, SerializedBuffer buf);
    void Stop();

private:
    int bridgeListenPort = 0;
    int bridgeFd = 0;
};
}
#endif