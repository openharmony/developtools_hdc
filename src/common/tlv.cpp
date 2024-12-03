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

#include "tlv.h"

using namespace std;

namespace Hdc {

TlvBuf::TlvBuf()
{
    this->Clear();
}

TlvBuf::TlvBuf(std::set<uint32_t> validtags)
{
    this->Clear();
    this->mValidTags = validtags;
}

TlvBuf::TlvBuf(const uint8_t *tlvs, const uint32_t size)
{
    this->Clear();
    if (tlvs == nullptr || size == 0) {
        WRITE_LOG(LOG_WARN, "invalid tlvs or size, size is %u", size);
        return;
    }
    for (uint32_t pos = 0; pos < size;) {
        if ((size - pos) < TLV_HEAD_SIZE) {
            WRITE_LOG(LOG_WARN, "invalid tlvs, size is %u, pos is %d", size, pos);
            this->Clear();
            return;
        }
        uint32_t tag = TO_TAG(tlvs + pos);
        pos += sizeof(tag);
        uint32_t len = TO_LEN(tlvs + pos);
        pos += sizeof(len);
        const uint8_t *val = tlvs + pos;

        if (len <= 0 || len > (size - pos)) {
            WRITE_LOG(LOG_WARN, "invalid tlvs, tag %u, len %u, pos %d, size %u", tag, len, pos, size);
            this->Clear();
            return;
        }
        if (!this->Append(tag, len, val)) {
            WRITE_LOG(LOG_WARN, "append tlv failed, tag %u, len %u", tag, len);
            this->Clear();
            return;
        }
        pos += len;
    }
}

TlvBuf::TlvBuf(const uint8_t *tlvs, const uint32_t size, const std::set<uint32_t> validtags) : TlvBuf(tlvs, size)
{
    this->mValidTags = validtags;
}

TlvBuf::~TlvBuf()
{
    this->Clear();
}

void TlvBuf::Clear(void)
{
    if (this->mTlvMap.empty()) {
        return;
    }

    for (auto it = this->mTlvMap.begin(); it != this->mTlvMap.end(); ++it) {
        it->second.clear();
    }
    this->mTlvMap.clear();
    this->mValidTags.clear();
}

bool TlvBuf::Append(const uint32_t tag, const string &val)
{
    return this->Append(tag, val.size(), reinterpret_cast<const uint8_t*>(val.data()));
}

bool TlvBuf::Append(const uint32_t tag, const uint32_t len, const uint8_t *val)
{
    if (len == 0) {
        WRITE_LOG(LOG_WARN, "the len is invalid: %u", len);
        return false;
    }
    if (val == nullptr) {
        WRITE_LOG(LOG_WARN, "the val ptr is null");
        return false;
    }
    if (this->mTlvMap.count(tag) > 0) {
        WRITE_LOG(LOG_WARN, "duplicate tag is %u", tag);
        return false;
    }
    std::vector<uint8_t> v;
    v.assign(val, val + len);
    this->mTlvMap[tag] = v;
    return true;
}

bool TlvBuf::FindTlv(const uint32_t tag, uint32_t &len, uint8_t *&val) const
{
    auto it = this->mTlvMap.find(tag);
    if (it == this->mTlvMap.end()) {
        return false;
    }
    auto tlv = it->second;
    len = tlv.size();
    if (len == 0) {
        WRITE_LOG(LOG_WARN, "invalid size 0 for tag %u", it->first);
        return false;
    }
    val = new uint8_t[len];
    if (val == nullptr) {
        WRITE_LOG(LOG_WARN, "memory not enough %u", len);
        return false;
    }
    if (memcpy_s(val, len, tlv.data(), tlv.size()) != 0) {
        WRITE_LOG(LOG_WARN, "memcpy failed, len %u", len);
        delete[] val;
        val = nullptr;
        len = 0;
        return false;
    }
    return true;
}

bool TlvBuf::FindTlv(const uint32_t tag, string &val) const
{
    auto it = this->mTlvMap.find(tag);
    if (it == this->mTlvMap.end()) {
        return false;
    }
    auto tlv = it->second;
    val.clear();
    val.assign(tlv.begin(), tlv.end());

    return true;
}

bool TlvBuf::ContainInvalidTag(void) const
{
    if (this->mTlvMap.empty() || this->mValidTags.empty()) {
        return false;
    }

    for (auto it = this->mTlvMap.begin(); it != this->mTlvMap.end(); ++it) {
        if (this->mValidTags.count(it->first) == 0) {
            WRITE_LOG(LOG_WARN, "contain invalid tag %u len %u", it->first, it->second.size());
            return true;
        }
    }
    return false;
}

uint32_t TlvBuf::GetBufSize(void) const
{
    if (this->mTlvMap.empty()) {
        return 0;
    }

    uint32_t size = 0;
    for (auto it = this->mTlvMap.begin(); it != this->mTlvMap.end(); ++it) {
        size += TLV_HEAD_SIZE;
        size += it->second.size();
    }
    return size;
}

bool TlvBuf::CopyToBuf(uint8_t *dst, const uint32_t size) const
{
    uint32_t mysize = this->GetBufSize();
    if (dst == nullptr || size < mysize) {
        WRITE_LOG(LOG_WARN, "invalid buf or size, size is %u my size is %u", size, mysize);
        return false;
    }

    uint32_t pos = 0;
    for (auto it = this->mTlvMap.begin(); it != this->mTlvMap.end(); ++it) {
        auto tlv = it->second;
        *(uint32_t *)(dst + pos) = it->first;
        pos += sizeof(uint32_t);
        *(uint32_t *)(dst + pos) = tlv.size();
        pos += sizeof(uint32_t);
        if (memcpy_s(dst + pos, size - pos, tlv.data(), tlv.size()) != 0) {
            WRITE_LOG(LOG_WARN, "memcpy failed, len %u", tlv.size());
            return false;
        }
        pos += tlv.size();
    }
    return true;
}

void TlvBuf::Display(void) const
{
    if (this->mTlvMap.empty()) {
        WRITE_LOG(LOG_INFO, "there is no tlv now");
        return;
    }
    for (auto it = this->mTlvMap.begin(); it != this->mTlvMap.end(); ++it) {
        WRITE_LOG(LOG_INFO, "tag %u len %u", it->first, it->second.size());
    }
}

} /* namespace Hdc */
