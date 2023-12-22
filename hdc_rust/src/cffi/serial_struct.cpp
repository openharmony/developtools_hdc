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
#include "serial_struct.h"
#include <iostream>
#include <cstring>

namespace Hdc {

char *StringToHeapPtr(std::string input) {
    size_t buf_size = input.length() + 1;
    char *buf_ret = (char *)malloc(buf_size);
    memset_s(buf_ret, buf_size, 0, buf_size);
    memcpy_s(buf_ret, buf_size, input.c_str(), buf_size);
    return buf_ret;
}

extern "C" SerializedBuffer SerializeSessionHandShake(const RustStruct::SessionHandShake &value) {
    BaseStruct::SessionHandShake shs = {
        .banner = string(value.banner),
        .authType = value.authType,
        .sessionId = value.sessionId,
        .connectKey = string(value.connectKey),
        .buf = string(value.buf),
        .version = string(value.version)
    };
    string serialized = Hdc::SerialStruct::SerializeToString(shs);
    size_t len = serialized.length();
    char *ptr = StringToHeapPtr(serialized);
    return SerializedBuffer{ptr, len};
}

extern "C" SerializedBuffer SerializePayloadProtect(const RustStruct::PayloadProtect &value) {
    BaseStruct::PayloadProtect pp = {
        .channelId = value.channelId,
        .commandFlag = value.commandFlag,
        .checkSum = value.checkSum,
        .vCode = value.vCode
    };
    string serialized = Hdc::SerialStruct::SerializeToString(pp);
    size_t len = serialized.length();
    char *ptr = StringToHeapPtr(serialized);
    return SerializedBuffer{ptr, len};
}

extern "C" SerializedBuffer SerializeTransferConfig(const RustStruct::TransferConfig &value) {
    BaseStruct::TransferConfig tc = {
        .fileSize = value.fileSize,
        .atime = value.atime,
        .mtime = value.mtime,
        .options = string(value.options),
        .path = string(value.path),
        .optionalName = string(value.optionalName),
        .updateIfNew = static_cast<bool>(value.updateIfNew),
        .compressType = value.compressType,
        .holdTimestamp = static_cast<bool>(value.holdTimestamp),
        .functionName = string(value.functionName),
        .clientCwd = string(value.clientCwd),
        .reserve1 = string(value.reserve1),
        .reserve2 = string(value.reserve2)
    };
    string serialized = Hdc::SerialStruct::SerializeToString(tc);
    size_t len = serialized.length();
    char *ptr = StringToHeapPtr(serialized);
    return SerializedBuffer{ptr, len};
}

extern "C" SerializedBuffer SerializeFileMode(const RustStruct::FileMode &value) {
    BaseStruct::FileMode fm = {
        .perm = value.perm,
        .u_id = value.u_id,
        .g_id = value.g_id,
        .context = string(value.context),
        .fullName = string(value.context)
    };
    string serialized = Hdc::SerialStruct::SerializeToString(fm);
    size_t len = serialized.length();
    char *ptr = StringToHeapPtr(serialized);
    return SerializedBuffer{ptr, len};
}

extern "C" SerializedBuffer SerializeTransferPayload(const RustStruct::TransferPayload &value) {
    BaseStruct::TransferPayload tp = {
        .index = value.index,
        .compressType = value.compressType,
        .compressSize = value.compressSize,
        .uncompressSize = value.uncompressSize
    };
    string serialized = Hdc::SerialStruct::SerializeToString(tp);
    size_t len = serialized.length();
    char *ptr = StringToHeapPtr(serialized);
    return SerializedBuffer{ptr, len};
}

extern "C" SerializedBuffer SerializePayloadHead(RustStruct::PayloadHead &value) {
    size_t len = sizeof(value);
    char *ptr = (char *)malloc(len);
    memcpy_s(ptr, len, reinterpret_cast<char *>(&value), len);
    return SerializedBuffer{ptr, len};
}

extern "C" SerializedBuffer SerializeUsbHead(RustStruct::USBHead &value) {
    size_t len = sizeof(value);
    char *ptr = (char *)malloc(len);
    memcpy_s(ptr, len, reinterpret_cast<char *>(&value), len);
    return SerializedBuffer{ptr, len};
}

extern "C" SerializedBuffer SerializeUartHead(RustStruct::UartHead &value) {
    size_t len = sizeof(value);
    char *ptr = (char *)malloc(len);
    memcpy_s(ptr, len, reinterpret_cast<char *>(&value), len);
    return SerializedBuffer{ptr, len};
}

extern "C" uint8_t ParseSessionHandShake(RustStruct::SessionHandShake &value, SerializedBuffer buf) {
    BaseStruct::SessionHandShake shs = {};
    if(!SerialStruct::ParseFromString(shs, string(buf.ptr, buf.size))) {
        return 0;
    }
    value = {
        .banner = StringToHeapPtr(shs.banner),
        .authType = shs.authType,
        .sessionId = shs.sessionId,
        .connectKey = StringToHeapPtr(shs.connectKey),
        .buf = StringToHeapPtr(shs.buf),
        .version = StringToHeapPtr(shs.version)
    };
    return 1;
}

extern "C" uint8_t ParsePayloadProtect(RustStruct::PayloadProtect &value, SerializedBuffer buf) {
    BaseStruct::PayloadProtect pp = {};
    if(!SerialStruct::ParseFromString(pp, string(buf.ptr, buf.size))) {
        return 0;
    }
    value = {
        .channelId = pp.channelId,
        .commandFlag = pp.commandFlag,
        .checkSum = pp.checkSum,
        .vCode = pp.vCode
    };
    return 1;
}

extern "C" uint8_t ParseTransferConfig(RustStruct::TransferConfig &value, SerializedBuffer buf) {
    BaseStruct::TransferConfig tc = {};
    if(!SerialStruct::ParseFromString(tc, string(buf.ptr, buf.size))) {
        return 0;
    }
    value = {
        .fileSize = tc.fileSize,
        .atime = tc.atime,
        .mtime = tc.mtime,
        .options = StringToHeapPtr(tc.options),
        .path = StringToHeapPtr(tc.path),
        .optionalName = StringToHeapPtr(tc.optionalName),
        .updateIfNew = tc.updateIfNew > 0,
        .compressType = tc.compressType,
        .holdTimestamp = tc.holdTimestamp > 0,
        .functionName = StringToHeapPtr(tc.functionName), // must first index
        .clientCwd = StringToHeapPtr(tc.clientCwd),
        .reserve1 = StringToHeapPtr(tc.reserve1),
        .reserve2 = StringToHeapPtr(tc.reserve2)
    };
    return 1;
}

extern "C" uint8_t ParseFileMode(RustStruct::FileMode &value, SerializedBuffer buf) {
    BaseStruct::FileMode fm = {};
    if(!SerialStruct::ParseFromString(fm, string(buf.ptr, buf.size))) {
        return 0;
    }
    value = {
        .perm = fm.perm,
        .u_id = fm.u_id,
        .g_id = fm.g_id,
        .context = StringToHeapPtr(fm.context),
        .fullName = StringToHeapPtr(fm.fullName)
    };
    return 1;
}

extern "C" uint8_t ParseTransferPayload(RustStruct::TransferPayload &value, SerializedBuffer buf) {
    BaseStruct::TransferPayload tp = {};
    if(!SerialStruct::ParseFromString(tp, string(buf.ptr, buf.size))) {
        return 0;
    }
    value = {
        .index = tp.index,
        .compressType = tp.compressType,
        .compressSize = tp.compressSize,
        .uncompressSize = tp.uncompressSize
    };
    return 1;
}

extern "C" uint8_t ParsePayloadHead(RustStruct::PayloadHead &value, SerializedBuffer buf) {
    memcpy_s(&value, buf.size, reinterpret_cast<struct PayloadHead *>(buf.ptr), buf.size);
    return 1;
}

extern "C" uint8_t ParseUsbHead(RustStruct::USBHead &value, SerializedBuffer buf) {
    memcpy_s(&value, buf.size, reinterpret_cast<struct USBHead *>(buf.ptr), buf.size);
    return 1;
}

extern "C" uint8_t ParseUartHead(RustStruct::UartHead &value, SerializedBuffer buf) {
    memcpy_s(&value, buf.size, reinterpret_cast<struct UartHead *>(buf.ptr), buf.size);
    return 1;
}

}; // Hdc
