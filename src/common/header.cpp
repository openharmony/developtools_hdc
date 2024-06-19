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

std::string DecimalToOctalString(int decimalNumber, int length)
{
    std::ostringstream oss;
    oss << std::oct << std::setw(length) << std::setfill('0') << decimalNumber;
    return oss.str();
}

Header::Header()
{
    (void)memset_s(name, HEADER_NAME_LEN, 0, HEADER_NAME_LEN);
    (void)memset_s(mode, HEADER_MODE_LEN, 0, HEADER_MODE_LEN);
    (void)memset_s(uid, HEADER_UID_LEN, 0, HEADER_UID_LEN);
    (void)memset_s(gid, HEADER_GID_LEN, 0, HEADER_GID_LEN);
    (void)memset_s(size, HEADER_SIZE_LEN, 0, HEADER_SIZE_LEN);
    (void)memset_s(mtime, HEADER_MTIME_LEN, 0, HEADER_MTIME_LEN);
    (void)memset_s(chksum, HEADER_CHKSUM_LEN, 0, HEADER_CHKSUM_LEN);
    (void)memset_s(typeflage, HEADER_TYPEFLAGE_LEN, 0, HEADER_TYPEFLAGE_LEN);
    (void)memset_s(linkname, HEADER_LINKNAME_LEN, 0, HEADER_LINKNAME_LEN);
    (void)memset_s(magic, HEADER_MAGIC_LEN, 0, HEADER_MAGIC_LEN);
    (void)memset_s(version, HEADER_VERSION_LEN, 0, HEADER_VERSION_LEN);
    (void)memset_s(uname, HEADER_UNAME_LEN, 0, HEADER_UNAME_LEN);
    (void)memset_s(gname, HEADER_GNAME_LEN, 0, HEADER_GNAME_LEN);
    (void)memset_s(devmajor, HEADER_DEVMAJOR_LEN, 0, HEADER_DEVMAJOR_LEN);
    (void)memset_s(devminor, HEADER_DEVMINOR_LEN, 0, HEADER_DEVMINOR_LEN);
    (void)memset_s(prefix, HEADER_PREFIX_LEN, 0, HEADER_PREFIX_LEN);
    (void)memset_s(pad, HEADER_PAD_LEN, 0, HEADER_PAD_LEN);
    (void)memcpy_s(magic, HEADER_MAGIC_LEN, MAGIC, HEADER_MAGIC_LEN);
    (void)memcpy_s(version, HEADER_VERSION_LEN, VERSION, HEADER_VERSION_LEN);
}

Header::Header(uint8_t data[512])
{
    int index = 0;
    auto func = [&data, &index](uint8_t target[], int len) {
        if (memcpy_s(target, len, &data[index], len) != EOK) {
            WRITE_LOG(LOG_WARN, "memcpy_s data failed index:%d len:%d", index, len);
        }
        index += len;
    };

    func(name, HEADER_NAME_LEN);
    func(mode, HEADER_MODE_LEN);
    func(uid, HEADER_UID_LEN);
    func(gid, HEADER_GID_LEN);
    func(size, HEADER_SIZE_LEN);
    func(mtime, HEADER_MTIME_LEN);
    func(chksum, HEADER_CHKSUM_LEN);
    func(typeflage, HEADER_TYPEFLAGE_LEN);
    func(linkname, HEADER_LINKNAME_LEN);
    func(magic, HEADER_MAGIC_LEN);
    func(version, HEADER_VERSION_LEN);
    func(uname, HEADER_UNAME_LEN);
    func(gname, HEADER_GNAME_LEN);
    func(devmajor, HEADER_DEVMAJOR_LEN);
    func(devminor, HEADER_DEVMINOR_LEN);
    func(prefix, HEADER_PREFIX_LEN);
    func(pad, HEADER_PAD_LEN);
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

size_t Header::Size()
{
    std::string octalStr(reinterpret_cast<char*>(this->size));
    int num = 0;
    if (!octalStr.empty()) {
        const int octal = 8;
        num = std::stoi(octalStr, nullptr, octal);
    }
    WRITE_LOG(LOG_INFO, "size num %d", num);
    return num;
}

void Header::UpdataSize(size_t fileLen)
{
    auto sizeStr = DecimalToOctalString(fileLen, HEADER_SIZE_LEN - 1);
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
    auto checksum = [&sum] (uint8_t *data, size_t len) {
        for (size_t i = 0; i < len; i++) {
            sum += data[i];
        }
    };
    checksum(this->name, HEADER_NAME_LEN);
    checksum(this->mode, HEADER_MODE_LEN);
    checksum(this->uid, HEADER_UID_LEN);
    checksum(this->gid, HEADER_GID_LEN);
    checksum(this->size, HEADER_SIZE_LEN);
    checksum(this->mtime, HEADER_MTIME_LEN);
    checksum(typeflage, HEADER_TYPEFLAGE_LEN);
    checksum(linkname, HEADER_LINKNAME_LEN);
    checksum(magic, HEADER_MAGIC_LEN);
    checksum(version, HEADER_VERSION_LEN);
    checksum(uname, HEADER_UNAME_LEN);
    checksum(gname, HEADER_GNAME_LEN);
    checksum(devmajor, HEADER_DEVMAJOR_LEN);
    checksum(devminor, HEADER_DEVMINOR_LEN);
    checksum(prefix, HEADER_PREFIX_LEN);
    checksum(pad, HEADER_PAD_LEN);
    constexpr uint64_t cnt = 256;
    sum += cnt;

    auto sizeStr = DecimalToOctalString(sum, HEADER_CHKSUM_LEN - 1);
    char *p = reinterpret_cast<char*>(this->chksum);
    int rc = snprintf_s(p, HEADER_CHKSUM_LEN, HEADER_CHKSUM_LEN - 1, "%s", sizeStr.c_str());
    if (rc < 0) {
        WRITE_LOG(LOG_WARN, "snprintf_s chksum failed rc:%d sizeStr:%s", rc, sizeStr.c_str());
    }
}

void Header::GetBytes(uint8_t data[512])
{
    UpdataCheckSum();
    int index = 0;
    auto func = [&index, &data](uint8_t *src, int len) {
        (void)memcpy_s(&data[index], len, src, len);
        index += len;
    };
    func(name, HEADER_NAME_LEN);
    func(mode, HEADER_MODE_LEN);
    func(uid, HEADER_UID_LEN);
    func(gid, HEADER_GID_LEN);
    func(size, HEADER_SIZE_LEN);
    func(mtime, HEADER_MTIME_LEN);
    func(chksum, HEADER_CHKSUM_LEN);
    func(typeflage, HEADER_TYPEFLAGE_LEN);
    func(linkname, HEADER_LINKNAME_LEN);
    func(magic, HEADER_MAGIC_LEN);
    func(version, HEADER_VERSION_LEN);
    func(uname, HEADER_UNAME_LEN);
    func(gname, HEADER_GNAME_LEN);
    func(devmajor, HEADER_DEVMAJOR_LEN);
    func(devminor, HEADER_DEVMINOR_LEN);
    func(prefix, HEADER_PREFIX_LEN);
    func(pad, HEADER_PAD_LEN);
}
}
