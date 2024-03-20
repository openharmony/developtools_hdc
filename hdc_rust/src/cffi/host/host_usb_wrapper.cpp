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
#include "host_usb.h"
#include "ffi_utils.h"

using namespace Hdc;

extern "C" void* InitHostUsb()
{
    HostUsb* ptr = new HostUsb();
    ptr->Initial();
    return ptr;
}

extern "C" PersistBuffer GetReadyUsbDevice(void* ptr)
{
    if (ptr == nullptr) {
        return PersistBuffer {
            reinterpret_cast<char *>(0),
            static_cast<uint64_t>(0)
        };
    }
    HostUsb* usbPtr = static_cast<HostUsb*>(ptr);
    if (usbPtr == nullptr) {
        return PersistBuffer {
            reinterpret_cast<char *>(0),
            static_cast<uint64_t>(0)
        };
    }
    HDaemonInfo pi;
    std::string ret = usbPtr->AdminDaemonMap(OP_GET_READY_STRLIST, "", pi);
    char* str = new char[ret.length()];
    if (memcpy_s(str, ret.length(), ret.c_str(), ret.length()) < 0) {
        return PersistBuffer {
            reinterpret_cast<char *>(0),
            static_cast<uint64_t>(0)
        };
    }
    return PersistBuffer {
        reinterpret_cast<char *>(str),
        static_cast<uint64_t>(ret.length())
    };
}

extern "C" void OnDeviceConnected(void* ptr, char* connectKey, int len, bool success)
{
    if (ptr == nullptr) {
        return;
    }
    HostUsb* usbPtr = static_cast<HostUsb*>(ptr);
    if (usbPtr == nullptr) {
        return;
    }
    char* key = new char[len + 1];
    memset_s(key, len + 1, 0, len + 1);
    if (memcpy_s(key, len + 1, connectKey, len) < 0) {
        return;
    }
    HUSB hUSB = usbPtr->GetUsbDevice(std::string(key));
    delete[] key;
    usbPtr->UpdateUSBDaemonInfo(hUSB, success ? STATUS_CONNECTED : STATUS_OFFLINE);
}

extern "C" int WriteUsb(void* ptr, char* connectKey, int len, SerializedBuffer buf)
{
    if (ptr == nullptr) {
        return -1;
    }
    HostUsb* usbPtr = static_cast<HostUsb*>(ptr);
    if (usbPtr == nullptr) {
        return -1;
    }
    char* key = new char[len + 1];
    memset_s(key, len + 1, 0, len + 1);
    if (memcpy_s(key, len + 1, connectKey, len) < 0) {
        return -1;
    }
    HUSB hUSB = usbPtr->GetUsbDevice(std::string(key));
    delete[] key;
    return usbPtr->WriteUsbIO(hUSB, buf);
}

extern "C" PersistBuffer ReadUsb(void* ptr, char* connectKey, int len, int exceptedSize)
{
    if (ptr == nullptr) {
        return PersistBuffer {
            reinterpret_cast<char *>(0),
            static_cast<uint64_t>(0)
        };
    }
    HostUsb* usbPtr = static_cast<HostUsb*>(ptr);
    if (usbPtr == nullptr) {
        return PersistBuffer {
            reinterpret_cast<char *>(0),
            static_cast<uint64_t>(0)
        };
    }
    char* key = new char[len + 1];
    memset_s(key, len + 1, 0, len + 1);
    if (memcpy_s(key, len + 1, connectKey, len) < 0) {
        return PersistBuffer {
            reinterpret_cast<char *>(0),
            static_cast<uint64_t>(0)
        };
    }
    HUSB hUSB = usbPtr->GetUsbDevice(std::string(key));
    delete[] key;
    return usbPtr->ReadUsbIO(hUSB, exceptedSize);
}

extern "C" void CancelUsbIo(void* ptr, char* connectKey, int len)
{
    if (ptr == nullptr) {
        return;
    }
    HostUsb* usbPtr = static_cast<HostUsb*>(ptr);
    if (usbPtr == nullptr) {
        return;
    }
    char* key = new char[len + 1];
    memset_s(key, len + 1, 0, len + 1);
    if (memcpy_s(key, len + 1, connectKey, len) < 0) {
        return;
    }
    HUSB hUSB = usbPtr->GetUsbDevice(std::string(key));
    delete[] key;
    usbPtr->CancelUsbIo(hUSB);
}

extern "C" bool Stop(void* ptr)
{
    if (ptr == nullptr) {
        return false;
    }
    HostUsb* usbPtr = static_cast<HostUsb*>(ptr);
    if (usbPtr == nullptr) {
        return false;
    }
    usbPtr->Stop();
    delete usbPtr;
    return true;
}