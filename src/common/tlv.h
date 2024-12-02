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

#ifndef __H_TLV_H__
#define __H_TLV_H__

struct TLV {
    uint32_t tag;
    uint32_t len;
    uint8_t *val;
};

class TlvBuf {
public:
    // construct a empty TlvBuf object with valid tags
    TlvBuf(std::set<uint32_t> validtags);
    // construct a TlvBuf object from a TLV buffer with valid tags
    TlvBuf(uint8_t *tlvs, uint32_t size, std::set<uint32_t> validtags);
    ~TlvBuf();
public:
    bool Append(const struct TLV *t, const uint32 size);
    bool CopyToBuf(uint8_t *dst, const uint32_t size) const;
    uint32_t GetBufSize(void) const;
    // the caller must free the memory pointed by the return value if not null
    struct TLV * FindTlv(uint32_t tag) const;
    // if return true, invalid_tags is empty, else the invalid tags will bring out by invalid_tags
    bool ContainInvalidTag(std::set<uint32_t> &invalid_tags) const;
    void Display(void) const;
private:
    std::map<uint32_t tag, struct TLV tlv> mTlvMap;
    std::set<uint32_t> mValidTags;
}

#endif
