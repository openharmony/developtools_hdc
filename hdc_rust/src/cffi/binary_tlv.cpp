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
#include "binary_tlv.h"
#include <securec.h>
#include <cstring>
#include <map>
#include <set>
#include <vector>

struct TlvBufImpl {
    std::map<uint32_t, std::vector<uint8_t>> tlvMap;
    std::set<uint32_t> validTags;
};

extern "C" {
void *TlvCreate()
{
    return new TlvBufImpl();
}

void TlvDestroy(void *buf)
{
    delete static_cast<TlvBufImpl *>(buf);
}

bool TlvAppend(void *buf, uint32_t tag, const uint8_t *val, uint32_t len)
{
    auto *tlv = static_cast<TlvBufImpl *>(buf);
    if (tlv == nullptr || val == nullptr || len == 0 || len > TLV_VALUE_MAX) {
        return false;
    }
    if (tlv->tlvMap.count(tag) > 0) {
        return false;
    }
    tlv->tlvMap[tag].assign(val, val + len);
    return true;
}

bool TlvAppendStr(void *buf, uint32_t tag, const char *val)
{
    if (val == nullptr) {
        return false;
    }
    return TlvAppend(buf, tag, reinterpret_cast<const uint8_t *>(val), strlen(val));
}

uint32_t TlvGetSize(const void *buf)
{
    const auto *tlv = static_cast<const TlvBufImpl *>(buf);
    if (tlv == nullptr || tlv->tlvMap.empty()) {
        return 0;
    }
    uint32_t size = 0;
    for (const auto &pair : tlv->tlvMap) {
        size += TLV_HEAD_SIZE;
        size += static_cast<uint32_t>(pair.second.size());
    }
    return size;
}

bool TlvCopyToBuf(const void *buf, uint8_t *dst, uint32_t size)
{
    const auto *tlv = static_cast<const TlvBufImpl *>(buf);
    if (tlv == nullptr || dst == nullptr) {
        return false;
    }
    uint32_t mySize = TlvGetSize(buf);
    if (size < mySize) {
        return false;
    }
    uint32_t pos = 0;
    for (const auto &pair : tlv->tlvMap) {
        if (memcpy_s(dst + pos, size - pos, &pair.first, sizeof(uint32_t)) != EOK) { return false; }
        pos += sizeof(uint32_t);
        uint32_t len = static_cast<uint32_t>(pair.second.size());
        if (memcpy_s(dst + pos, size - pos, &len, sizeof(uint32_t)) != EOK) { return false; }
        pos += sizeof(uint32_t);
        if (memcpy_s(dst + pos, size - pos, pair.second.data(), len) != EOK) { return false; }
        pos += len;
    }
    return true;
}

bool TlvFind(const void *buf, uint32_t tag, uint32_t *len, uint8_t *val, uint32_t maxLen)
{
    const auto *tlv = static_cast<const TlvBufImpl *>(buf);
    if (tlv == nullptr) {
        return false;
    }
    auto it = tlv->tlvMap.find(tag);
    if (it == tlv->tlvMap.end()) {
        return false;
    }
    if (len != nullptr) {
        *len = static_cast<uint32_t>(it->second.size());
    }
    if (val != nullptr && maxLen >= it->second.size()) {
        if (memcpy_s(val, maxLen, it->second.data(), it->second.size()) != EOK) { return false; }
    }
    return true;
}

void TlvClear(void *buf)
{
    auto *tlv = static_cast<TlvBufImpl *>(buf);
    if (tlv == nullptr) {
        return;
    }
    tlv->tlvMap.clear();
    tlv->validTags.clear();
}

bool TlvContainInvalid(const void *buf)
{
    const auto *tlv = static_cast<const TlvBufImpl *>(buf);
    if (tlv == nullptr || tlv->tlvMap.empty() || tlv->validTags.empty()) {
        return false;
    }
    for (const auto &pair : tlv->tlvMap) {
        if (tlv->validTags.count(pair.first) == 0) {
            return true;
        }
    }
    return false;
}
}
