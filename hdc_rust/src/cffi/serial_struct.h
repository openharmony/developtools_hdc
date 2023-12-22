/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#ifndef HDC_SERIAL_STRUCT_H
#define HDC_SERIAL_STRUCT_H

#include "serial_struct_define.h"

#include <string>
#include <sstream>

using std::string;

namespace Hdc {
namespace BaseStruct {
    struct SessionHandShake {
        string banner; // must first index
        // auth none
        uint8_t authType;
        uint32_t sessionId;
        string connectKey;
        string buf;
        string version;
        std::string ToDebugString()
        {
            std::ostringstream oss;
            oss << "SessionHandShake [";
            oss << " banner:" << banner;
            oss << " sessionId:" << sessionId;
            oss << " authType:" << unsigned(authType);
            oss << " connectKey:" << connectKey;
            oss << " buf:" << buf;
            oss << " version:" << version;
            oss << " ]";
            return oss.str();
        }
    };

    struct PayloadProtect {  // reserve for encrypt and decrypt
        uint32_t channelId;
        uint32_t commandFlag;
        uint8_t checkSum;  // enable it will be lose about 20% speed
        uint8_t vCode;
    };

    struct TransferConfig {
        uint64_t fileSize;
        uint64_t atime;  // ns
        uint64_t mtime;  // ns
        string options;
        string path;
        string optionalName;
        bool updateIfNew;
        uint8_t compressType;
        bool holdTimestamp;
        string functionName;
        string clientCwd;
        string reserve1;
        string reserve2;
    };

    struct FileMode {
        uint64_t perm;
        uint64_t u_id;
        uint64_t g_id;
        string context;
        string fullName;
    };

    // used for HdcTransferBase. just base class use, not public
    struct TransferPayload {
        uint64_t index;
        uint8_t compressType;
        uint32_t compressSize;
        uint32_t uncompressSize;
    };
} // BaseStruct

namespace RustStruct {
    struct SessionHandShake {
        const char* banner; // must first index
        // auth none
        uint8_t authType;
        uint32_t sessionId;
        const char* connectKey;
        const char* buf;
        const char* version;
    };

    struct PayloadProtect {  // reserve for encrypt and decrypt
        uint32_t channelId;
        uint32_t commandFlag;
        uint8_t checkSum;  // enable it will be lose about 20% speed
        uint8_t vCode;
    };

    struct TransferConfig {
        uint64_t fileSize;
        uint64_t atime;  // ns
        uint64_t mtime;  // ns
        const char* options;
        const char* path;
        const char* optionalName;
        uint8_t updateIfNew;
        uint8_t compressType;
        uint8_t holdTimestamp;
        const char* functionName;
        const char* clientCwd;
        const char* reserve1;
        const char* reserve2;
    };

    struct FileMode {
        uint64_t perm;
        uint64_t u_id;
        uint64_t g_id;
        const char* context;
        const char* fullName;
    };

    struct TransferPayload {
        uint64_t index;
        uint8_t compressType;
        uint32_t compressSize;
        uint32_t uncompressSize;
    };

    struct PayloadHead {
        uint8_t flag[2];
        uint8_t reserve[2];  // encrypt'flag or others options
        uint8_t protocolVer;
        uint16_t headSize;
        uint32_t dataSize;
    } __attribute__((packed));

    struct USBHead {
        uint8_t flag[2];
        uint8_t option;
        uint32_t sessionId;
        uint32_t dataSize;
    } __attribute__((packed));

    struct UartHead {
        uint8_t flag[2];
        uint16_t option;
        uint32_t sessionId;
        uint32_t dataSize;
        uint32_t package_index;
        uint32_t data_checksum;
        uint32_t head_checksum;
    } __attribute__((packed));
} // RustStruct

namespace SerialStruct {
    constexpr int fieldOne = 1;
    constexpr int fieldTwo = 2;
    constexpr int fieldThree = 3;
    constexpr int fieldFour = 4;
    constexpr int fieldFive = 5;
    constexpr int fieldSix = 6;
    constexpr int fieldSeven = 7;
    constexpr int fieldEight = 8;
    constexpr int fieldNine = 9;
    constexpr int fieldTen = 10;
    constexpr int field11 = 11;
    constexpr int field12 = 12;
    constexpr int field13 = 13;

    template<> struct Descriptor<BaseStruct::TransferConfig> {
        static auto type()
        {
            return Message(Field<fieldOne, &BaseStruct::TransferConfig::fileSize>("fileSize"),
                           Field<fieldTwo, &BaseStruct::TransferConfig::atime>("atime"),
                           Field<fieldThree, &BaseStruct::TransferConfig::mtime>("mtime"),
                           Field<fieldFour, &BaseStruct::TransferConfig::options>("options"),
                           Field<fieldFive, &BaseStruct::TransferConfig::path>("path"),
                           Field<fieldSix, &BaseStruct::TransferConfig::optionalName>("optionalName"),
                           Field<fieldSeven, &BaseStruct::TransferConfig::updateIfNew>("updateIfNew"),
                           Field<fieldEight, &BaseStruct::TransferConfig::compressType>("compressType"),
                           Field<fieldNine, &BaseStruct::TransferConfig::holdTimestamp>("holdTimestamp"),
                           Field<fieldTen, &BaseStruct::TransferConfig::functionName>("functionName"),
                           Field<field11, &BaseStruct::TransferConfig::clientCwd>("clientCwd"),
                           Field<field12, &BaseStruct::TransferConfig::reserve1>("reserve1"),
                           Field<field13, &BaseStruct::TransferConfig::reserve2>("reserve2"));
        }
    };

    template<> struct Descriptor<BaseStruct::FileMode> {
        static auto type()
        {
            return Message(Field<fieldOne, &BaseStruct::FileMode::perm>("perm"),
                           Field<fieldTwo, &BaseStruct::FileMode::u_id>("u_id"),
                           Field<fieldThree, &BaseStruct::FileMode::g_id>("g_id"),
                           Field<fieldFour, &BaseStruct::FileMode::context>("context"),
                           Field<fieldFive, &BaseStruct::FileMode::fullName>("fullName"));
        }
    };

    template<> struct Descriptor<BaseStruct::TransferPayload> {
        static auto type()
        {
            return Message(Field<fieldOne, &BaseStruct::TransferPayload::index>("index"),
                           Field<fieldTwo, &BaseStruct::TransferPayload::compressType>("compressType"),
                           Field<fieldThree, &BaseStruct::TransferPayload::compressSize>("compressSize"),
                           Field<fieldFour, &BaseStruct::TransferPayload::uncompressSize>("uncompressSize"));
        }
    };

    template<> struct Descriptor<BaseStruct::SessionHandShake> {
        static auto type()
        {
            return Message(Field<fieldOne, &BaseStruct::SessionHandShake::banner>("banner"),
                           Field<fieldTwo, &BaseStruct::SessionHandShake::authType>("authType"),
                           Field<fieldThree, &BaseStruct::SessionHandShake::sessionId>("sessionId"),
                           Field<fieldFour, &BaseStruct::SessionHandShake::connectKey>("connectKey"),
                           Field<fieldFive, &BaseStruct::SessionHandShake::buf>("buf"),
                           Field<fieldSix, &BaseStruct::SessionHandShake::version>("version"));
        }
    };

    template<> struct Descriptor<BaseStruct::PayloadProtect> {
        static auto type()
        {
            return Message(Field<fieldOne, &BaseStruct::PayloadProtect::channelId>("channelId"),
                           Field<fieldTwo, &BaseStruct::PayloadProtect::commandFlag>("commandFlag"),
                           Field<fieldThree, &BaseStruct::PayloadProtect::checkSum>("checkSum"),
                           Field<fieldFour, &BaseStruct::PayloadProtect::vCode>("vCode"));
        }
    };
}  // SerialStruct
}  // Hdc
#endif  // HDC_SERIAL_STRUCT_H
