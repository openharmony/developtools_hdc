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
#include "ffi_utils.h"
#include "oh_usb.h"
#include "usb_util.h"
#include "log.h"

#include <string>

namespace Hdc {
extern "C" int32_t ConfigEpPointEx(const char* path)
{
    int ep;
    if (ConfigEpPoint(ep, std::string(path)) != 0) {
        WRITE_LOG(LOG_WARN, "open ep failed");
        return -1;
    }
    return static_cast<int32_t>(ep);
}

extern "C" int32_t OpenEpPointEx(const char* path)
{
    int fd = -1;
    if (OpenEpPoint(fd, std::string(path)) != 0) {
        WRITE_LOG(LOG_WARN, "open ep failed");
        return -1;
    }
    return static_cast<int32_t>(fd);
}

extern "C" int32_t CloseUsbFdEx(int32_t fd)
{
    return static_cast<int32_t>(CloseUsbFd(fd));
}

extern "C" void CloseEndPointEx(int32_t bulkInFd, int32_t bulkOutFd, int32_t ctrlEp,
                                uint8_t closeCtrlEp)
{
    CloseEndpoint(bulkInFd, bulkOutFd, ctrlEp, closeCtrlEp);
}

extern "C" int32_t WriteUsbDevEx(int32_t bulkOut, SerializedBuffer buf)
{
    return static_cast<int32_t>(WriteData(bulkOut, reinterpret_cast<uint8_t *>(buf.ptr),
                                          static_cast<size_t>(buf.size)));
}

uint8_t *g_bufRet = nullptr;

struct PersistBuffer {
    uint8_t *ptr;
    uint64_t size;
};

extern "C" size_t ReadUsbDevEx(int32_t bulkIn, uint8_t *buf, const size_t size)
{
    return ReadData(bulkIn, buf, size);
}

extern "C" char *GetDevPathEx(const char *path)
{
    std::string basePath = GetDevPath(std::string(path));
    size_t buf_size = basePath.length() + 1;
    char *bufRet = new char[buf_size];
    (void)memset_s(bufRet, buf_size, 0, buf_size);
    (void)memcpy_s(bufRet, buf_size, basePath.c_str(), buf_size);
    return bufRet;
}

}
