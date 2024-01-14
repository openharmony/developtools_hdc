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

#include <string>

namespace Hdc {

constexpr uint16_t MAX_SIZE_IOBUF = 0xf000;

extern "C" int32_t ConfigEpPointEx(const char* path)
{
    int ep;
    if (ConfigEpPoint(ep, std::string(path)) != 0) {
        printf("open ep failed");
        return -1;
    }
    return static_cast<int32_t>(ep);
}

extern "C" int32_t OpenEpPointEx(const char* path)
{
    int fd = -1;
    if (OpenEpPoint(fd, std::string(path)) != 0) {
        printf("open ep failed");
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
    char *ptr;
    uint64_t size;
};

extern "C" PersistBuffer ReadUsbDevEx(int32_t bulkIn)
{
    if (g_bufRet == nullptr) {
        printf("remalloc g_bufRet\n");
        g_bufRet = new uint8_t[MAX_SIZE_IOBUF];
    }

    while (true) {
        int length = ReadData(bulkIn, g_bufRet, MAX_SIZE_IOBUF);
        if (length > 0) {
            return PersistBuffer{reinterpret_cast<char *>(g_bufRet), static_cast<uint64_t>(length)};
        } else if (length < 0) {
            return PersistBuffer{reinterpret_cast<char *>(g_bufRet), static_cast<uint64_t>(0)};
        }
    }
}

extern "C" char *GetDevPathEx(const char *path)
{
    std::string basePath = GetDevPath(std::string(path));
    size_t buf_size = basePath.length() + 1;
    char *bufRet = new char[buf_size];
    memset_s(bufRet, buf_size, 0, buf_size);
    memcpy_s(bufRet, buf_size, basePath.c_str(), buf_size);
    return bufRet;
}

}
