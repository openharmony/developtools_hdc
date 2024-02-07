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
#include "uart.h"

#include <string>

namespace Hdc {

extern "C" int32_t GetUartSpeedExt(int32_t speed)
{
    return static_cast<int32_t>(GetUartSpeed(static_cast<int>(speed)));
}

extern "C" int32_t GetUartBitsExt(int32_t bits)
{
    return static_cast<int32_t>(GetUartBits(static_cast<int>(bits)));
}

extern "C" int32_t OpenSerialPortExt(const char* portName)
{
    return static_cast<int32_t>(OpenSerialPort(std::string(portName)));
}

extern "C" int32_t SetSerialExt(int32_t fd, int32_t nSpeed, int32_t nBits, uint8_t nEvent, int32_t nStop)
{
    return static_cast<int32_t>(SetSerial(static_cast<int>(fd), static_cast<int>(nSpeed),
                                          static_cast<int>(nBits), static_cast<char>(nEvent),
                                          static_cast<int>(nStop)));
}

extern "C" SerializedBuffer ReadUartDevExt(int32_t fd, uint32_t expectedSize)
{
    std::vector<uint8_t> readBuf;
    ssize_t length = 0;
    while (length == 0) {
        length = ReadUartDev(static_cast<int>(fd), readBuf, static_cast<size_t>(expectedSize));
    }

    char *bufRet = new char;
    (void)memset_s(bufRet, length, 0, length);
    if (memcpy_s(bufRet, length, readBuf.data(), length) != EOK) {
        return SerializedBuffer{bufRet, static_cast<uint64_t>(length)};
    }
    return SerializedBuffer{bufRet, static_cast<uint64_t>(length)};
}

extern "C" int32_t WriteUartDevExt(int32_t fd, SerializedBuffer buf)
{
    return static_cast<int32_t>(WriteUartDev(static_cast<int>(fd), reinterpret_cast<uint8_t *>(buf.ptr),
                                             static_cast<size_t>(buf.size)));
}

extern "C" uint8_t CloseSerialPortExt(int32_t fd)
{
    return static_cast<uint8_t>(CloseSerialPort(fd));
}

}
