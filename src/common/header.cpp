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
#include "header.h"
#include <cstring>
#include <sstream>
#include <string>
#include <iomanip>

namespace Hdc {
constexpr uint8_t MAGIC[HEADER_MAGIC_LEN] = {'u', 's', 't', 'a', 'r', 0x20};
constexpr uint8_t VERSION[HEADER_VERSION_LEN] = {0x20, 0x00};

std::string DecimalToOctalString(size_t decimalNumber, int length)
{
    std::ostringstream oss;
    oss << std::oct << std::setw(length) << std::setfill('0') << decimalNumber;
    return oss.str();
}

Header::Header()
{
    (void)memset_s(this, sizeof(struct Header), 0, sizeof(struct Header));
    (void)memcpy_s(magic, HEADER_MAGIC_LEN, MAGIC, HEADER_MAGIC_LEN);
    (void)memcpy_s(version, HEADER_VERSION_LEN, VERSION, HEADER_VERSION_LEN);
}

Header::Header(uint8_t data[512], int dataLen)
{
    if (memcpy_s(this, sizeof(struct Header), data, dataLen) != EOK) {
        string tmp(reinterpret_cast<char*>(data), dataLen);
        WRITE_LOG(LOG_WARN, "memcpy_s failed for %s", tmp.c_str());
    }
}

std::string Header::Name()
{
    std::string fullName(reinterpret_cast<char*>(prefix));
    fullName.append(reinterpret_cast<char*>(this->name));
    return fullName;
}

bool Header::UpdataName(std::string fileName)
{
    auto len = fileName.length();
    if (len >= HEADER_MAX_FILE_LEN) {
        WRITE_LOG(LOG_WARN, "len too long %u", len);
        return false;
    }
    int rc = 0;
    char *p = nullptr;
    if (len < HEADER_NAME_LEN) {
        p = reinterpret_cast<char*>(this->name);
        rc = snprintf_s(p, HEADER_NAME_LEN, HEADER_NAME_LEN - 1, "%s", fileName.c_str());
        if (rc < 0) {
            WRITE_LOG(LOG_WARN, "snprintf_s name failed rc:%d p_name:%s", rc, fileName.c_str());
        }
    } else {
        auto sprefix = fileName.substr(0, len - (HEADER_NAME_LEN - 1));
        auto sname = fileName.substr(len - (HEADER_NAME_LEN - 1));
        p = reinterpret_cast<char*>(this->name);
        rc = snprintf_s(p, HEADER_NAME_LEN, HEADER_NAME_LEN - 1, "%s", sname.c_str());
        if (rc < 0) {
            WRITE_LOG(LOG_WARN, "snprintf_s name failed rc:%d sname:%s", rc, sname.c_str());
        }
        p = reinterpret_cast<char*>(this->prefix);
        rc = snprintf_s(p, HEADER_NAME_LEN, HEADER_NAME_LEN - 1, "%s", sprefix.c_str());
        if (rc < 0) {
            WRITE_LOG(LOG_WARN, "snprintf_s prefix failed rc:%d sprefix:%s", rc, sprefix.c_str());
        }
    }
    return true;
}

uint64_t Header::Size()
{
    std::string octalStr(reinterpret_cast<char*>(this->size), (HEADER_SIZE_LEN - 1));
    uint64_t num = 0;
    WRITE_LOG(LOG_DEBUG, "header size octalStr %s", octalStr.c_str());
    if (!octalStr.empty()) {
        const int octal = 8;
        if (std::find_if(octalStr.begin(), octalStr.end(),
            [](unsigned char c) { return c < '0' || c > '7'; }) == octalStr.end()) {
            num = stoull(octalStr, nullptr, octal);
        } else {
            num = 0;
            WRITE_LOG(LOG_WARN, "header size %s is invaild", octalStr.c_str());
        }
    }
    WRITE_LOG(LOG_DEBUG, "header size num %llu", num);
    return num;
}

void Header::UpdataSize(size_t fileLen)
{
    auto sizeStr = DecimalToOctalString(fileLen, HEADER_SIZE_LEN - 1);
    WRITE_LOG(LOG_DEBUG, "UpdataSize sizeStr %s", sizeStr.c_str());
    char *p = reinterpret_cast<char*>(this->size);
    int rc = snprintf_s(p, HEADER_SIZE_LEN, HEADER_SIZE_LEN - 1, "%s", sizeStr.c_str());
    if (rc < 0) {
        WRITE_LOG(LOG_FATAL, "snprintf_s size failed rc:%d sizeStr:%s", rc, sizeStr.c_str());
    }
}

TypeFlage Header::FileType()
{
    if (this->typeflage[0] < TypeFlage::ORDINARYFILE || this->typeflage[0] > TypeFlage::RESERVE) {
        return TypeFlage::INVALID;
    }

    return TypeFlage(this->typeflage[0]);
}

void Header::UpdataFileType(TypeFlage fileType)
{
    if (fileType < TypeFlage::ORDINARYFILE || fileType > TypeFlage::RESERVE) {
        this->typeflage[0] = TypeFlage::INVALID;
        return;
    }
    this->typeflage[0] = fileType;
}

bool Header::IsInvalid()
{
    return FileType() == TypeFlage::INVALID;
}

void Header::UpdataCheckSum()
{
    uint64_t sum = 0;
    uint8_t *tmp = reinterpret_cast<uint8_t*>(this);
    for (size_t i = 0; i < sizeof(struct Header); i++) {
        sum += tmp[i];
    }
    constexpr uint64_t cnt = 256;
    sum += cnt;

    auto sizeStr = DecimalToOctalString(sum, HEADER_CHKSUM_LEN - 1);
    char *p = reinterpret_cast<char*>(this->chksum);
    int rc = snprintf_s(p, HEADER_CHKSUM_LEN, HEADER_CHKSUM_LEN - 1, "%s", sizeStr.c_str());
    if (rc < 0) {
        WRITE_LOG(LOG_WARN, "snprintf_s chksum failed rc:%d sizeStr:%s", rc, sizeStr.c_str());
    }
}

void Header::GetBytes(uint8_t data[512], int dataLen)
{
    UpdataCheckSum();
    errno_t ret = memcpy_s(data, dataLen, this, sizeof(struct Header));
    if (ret != EOK) {
        WRITE_LOG(LOG_FATAL, "memcpy_s failed, errno:%d", ret);
    }
}

}