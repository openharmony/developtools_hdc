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

TlvBuf::TlvBuf(std::set<uint32_t> validtags)
{
    mValidTags = validtags;
    this->Clear();
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
        if (it->second.val != nullptr) {
            delete[] it->second.val;
        }
    }
    this->mTlvMap.clear();
}

TlvBuf::TlvBuf(uint8_t *tlvs, uint32_t size, std::set<uint32_t> validtags)
{
    this->Clear();
    if (tlvs == nullptr || size == 0) {
        WRITE_LOG(LOG_WARN, "invalid tlvs or size, size is %u", size);
        return;
    }
    for (uint32_t pos = 0; pos < size;) {
        if ((size - pos) < sizeof(struct TLV)) {
            WRITE_LOG(LOG_WARN, "invalid tlvs, size is %u, pos is %d", size, pos);
            this->Clear();
            return;
        }
        struct TLV *t = (struct TLV *)(tlvs + pos);
        pos += sizeof(struct TLV);
        uint32_t tag = t->tag;
        uint32_t len = t->len;

        if (len <= 0 || len > (size - pos)) {
            WRITE_LOG(LOG_WARN, "invalid tlvs, tag %u, len %u, pos %d, size %u", tag, len, pos, size);
            this->Clear();
            return;
        }
        if (!this->Append(tag, len, t->val)) {
            WRITE_LOG(LOG_WARN, "append tlv failed, tag %u, len %u", tag, len);
            this->Clear();
            return;
        }
    }
    this->mValidTags = validtags;
}

bool TlvBuf::Append(const struct TLV *t, const uint32_t size)
{
    if (t == nullptr || size < sizeof(struct TLV)) {
        WRITE_LOG(LOG_WARN, "invalid tlv or size, size is %u", size);
        return false;
    }
    if (t->len + sizeof(struct TLV) != size) {
        WRITE_LOG(LOG_WARN, "invalid size %u, len %u", size, t->len);
        return false;
    }

    return this->Append(t->tag, t->len, t->val);
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
    uint8_t *v = new uint8_t[len];
    if (v == nullptr) {
        WRITE_LOG(LOG_WARN, "memory not enough %u", len);
        return false;
    }
    if (memcpy_s(v, len, val, len) != 0) {
        WRITE_LOG(LOG_WARN, "memcpy failed, len %u", len);
        delete[] v;
        return false;
    }
    if (this->mTlvMap.count(tag) > 0) {
        WRITE_LOG(LOG_WARN, "duplicate tag is %u", tag);
        return false;
    }
    this->mTlvMap[tag] = {tag, len, v};
    return true;
}

struct TLV *TlvBuf::FindTlv(const uint32_t tag) const
{
    auto it = this->mTlvMap.find(tag);
    if (it == this->mTlvMap.end()) {
        return nullptr;
    }
    struct TLV t = it->second;
    struct TLV *ret = new TLV();
    if (ret == nullptr) {
        WRITE_LOG(LOG_WARN, "memory not enough");
        return nullptr;
    }
    ret->val = new uint8_t[t.len];
    if (ret == nullptr) {
        WRITE_LOG(LOG_WARN, "memory not enough %u", t.len);
        delete ret;
        return nullptr;
    }
    ret->tag = t.tag;
    ret->len = t.len;
    if (memcpy_s(ret->val, t.len, t.val, t.len) != 0) {
        WRITE_LOG(LOG_WARN, "memcpy failed, len %u", t.len);
        delete[] ret->val;
        delete ret;
        return nullptr;
    }
    return ret;
}

bool TlvBuf::ContainInvalidTag(void) const
{
    if (this->mTlvMap.empty() || this->mValidTags.empty()) {
        return false;
    }

    for (auto it = this->mTlvMap.begin(); it != this->mTlvMap.end(); ++it) {
        if (this->mValidTags.count(it->second.tag) != 0) {
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
        size += sizeof(it->second.tag);
        size += sizeof(it->second.len);
        size += it->second.len;
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
        struct TLV tlv = it->second;
        struct TLV *t = (struct TLV*)(dst + pos);
        t->tag = tlv.tag;
        pos += sizeof(t->tag);
        t->len = tlv.len;
        pos += sizeof(t->len);
        if (memcpy_s(dst + pos, size - pos, tlv.val, tlv.len) != 0) {
            WRITE_LOG(LOG_WARN, "memcpy failed, len %u", tlv.len);
            return false;
        }
        pos += tlv.len;
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
        struct TLV tlv = it->second;
        WRITE_LOG(LOG_INFO, "tag %u len %u", tlv.tag, tlv.len);
    }
}

} /* namespace Hdc */
