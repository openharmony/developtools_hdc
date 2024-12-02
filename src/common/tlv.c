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

namespace Hdc {

class TlvBuf {
public:
    // construct a empty TlvBuf object with valid tags
    TlvBuf(std::set<uint32_t> validtags);
    // construct a TlvBuf object from a TLV buffer with valid tags
    TlvBuf(uint8_t *tlvs, uint32_t size, std::set<uint32_t> validtags);
    ~TlvBuf();
public:
    bool Append(const struct TLV *t, const uint32 size);
    bool Append(const uint32_t tag, const uint32_t len, const uint8_t *val);
    bool CopyToBuf(uint8_t *dst, const uint32_t size) const;
    uint32_t GetBufSize(void) const;
    // the caller must free the memory pointed by the return value if not null
    struct TLV * FindTlv(uint32_t tag) const;
    // if return true, invalid_tags is empty, else the invalid tags will bring out by invalid_tags
    bool ContainInvalidTag(std::set<uint32_t> &invalid_tags) const;
    void Display(void) const;
private:
    void ClearBuf(void);
private:
    std::map<uint32_t /* tag */, struct TLV /* t/l/v */> mTlvMap;
}

TlvBuf::TlvBuf(std::set<uint32_t> validtags)
{
    mValidTags = validtags;
}

TlvBuf::TlvBuf(uint8_t *tlvs, uint32_t size, std::set<uint32_t> validtags)
{
    this->ClearBuf();
    if (tlvs == nullptr || size == 0) {
        WRITE_LOG(LOG_WARN, "invalid tlvs or size, size is %u", size);
        return;
    }
    for (int pos = 0; pos < size;) {
        if ((size - pos) < sizeof(struct TLV)) {
            WRITE_LOG(LOG_WARN, "invalid tlvs, size is %u, pos is %d", size, pos);
            this->ClearBuf();
            return;
        }
        struct TLV *t = (struct TLV *)(tlvs + pos);
        pos += sizeof(struct TLV);
        uint32_t tag = t->tag;
        uint32_t len = t->len;

        if (len <= 0 || len > (size - pos)) {
            WRITE_LOG(LOG_WARN, "invalid tlvs, tag %u, len %u, pos %d, size %u", t->tag, t->len, pos, size);
            this->ClearBuf();
            return;
        }
        if (!this->Append(t->tag, t->len, t->val)) {
            WRITE_LOG(LOG_WARN, "append tlv failed, tag %u, len %u", t->tag, t->len);
            this->ClearBuf();
            return;
        }
    }
    this->mValidTags = validtags;
}

bool TlvBuf::Append(const struct TLV *t, const uint32 size)
{
    if (t == nullptr || size < sizeof(struct TLV)) {
        WRITE_LOG(LOG_WARN, "invalid tlv or size, size is %u", size);
        return false;
    }
    if (t->len + sizeof(struct TLV) != size) {
        WRITE_LOG(LOG_WARN, "invalid size %u, len %u", size, t->len);
        return false;
    }

    return this->Append(t-tag, t->len, t->val);
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
    if (this->mTlvMap.count(t->tag) > 0) {
        WRITE_LOG(LOG_WARN, "duplicate tag is %u", t->tag);
        return false;
    }
    this->mTlvMap[tag] = {tag, len, v};
    return true;
}

} /* namespace Hdc */
