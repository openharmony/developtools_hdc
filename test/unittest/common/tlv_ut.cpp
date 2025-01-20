/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "tlv_ut.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing::ext;
using namespace testing;

namespace Hdc {
#define MAX_BUFFER_SIZE (1 * 1024 * 1024 * 1024)
class HdcTLVTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
private:
    uint8_t *BuildTlv(const std::vector<uint32_t> tags,
            const std::vector<uint32_t> sizes, uint8_t val, uint32_t &tlvsize) const;
            
    uint32_t TlvSize(uint32_t len) { return len + TLV_HEAD_SIZE; }
};

void HdcTLVTest::SetUpTestCase()
{
}

void HdcTLVTest::TearDownTestCase() {}

void HdcTLVTest::SetUp() {}

void HdcTLVTest::TearDown() {}

uint8_t *HdcTLVTest::BuildTlv(const std::vector<uint32_t> tags,
    const std::vector<uint32_t> sizes,
    uint8_t val,
    uint32_t &tlvsize) const
{
    if (tags.size() != sizes.size() || tags.empty()) {
        WRITE_LOG(LOG_WARN, "not valid size: %u, %u", tags.size(), sizes.size());
        return nullptr;
    }
    tlvsize = 0;
    for (auto size : sizes) {
        tlvsize += TLV_HEAD_SIZE;
        tlvsize += size;
    }

    if (tlvsize == 0 || tlvsize > MAX_BUFFER_SIZE) {
        WRITE_LOG(LOG_WARN, "invalid size 0");
        return nullptr;
    }
    uint8_t *tlv = new (std::nothrow) uint8_t[tlvsize];
    if (tlv == nullptr) {
        WRITE_LOG(LOG_WARN, "not enough memory %u", tlvsize);
        tlvsize = 0;
        return nullptr;
    }
    uint32_t pos = 0;
    int i = 0;
    for (; pos < tlvsize && i < tags.size(); i++) {
        *(uint32_t *)(tlv + pos) = tags[i];
        *(uint32_t *)(tlv + pos + sizeof(tags[i])) = sizes[i];

        pos += TLV_HEAD_SIZE;
        if (memset_s(tlv + pos, tlvsize - pos, val, sizes[i]) != 0) {
            tlvsize = 0;
            delete[] tlv;
            return nullptr;
        }
        pos += sizes[i];
    }

    return tlv;
}

HWTEST_F(HdcTLVTest, TlvBuf_Constructor_001, TestSize.Level0)
{
    TlvBuf tb;
    ASSERT_EQ(tb.GetBufSize(), 0);
}

HWTEST_F(HdcTLVTest, TlvBuf_Constructor_002, TestSize.Level0)
{
    std::set<uint32_t> validtags = { 1, 2, 3 };
    TlvBuf tb(validtags);
    ASSERT_EQ(tb.GetBufSize(), 0);
}

HWTEST_F(HdcTLVTest, TlvBuf_Constructor_003, TestSize.Level0)
{
    std::vector<uint32_t> tags = { 1 };
    std::vector<uint32_t> sizes = { 1 };
    uint32_t tlvsize = 0;
    uint8_t *tlv = this->BuildTlv(tags, sizes, 0xAB, tlvsize);
    ASSERT_NE(tlv, nullptr);

    TlvBuf tb(tlv, tlvsize);
    ASSERT_EQ(tb.GetBufSize(), tlvsize);

    delete[] tlv;
}

HWTEST_F(HdcTLVTest, TlvBuf_Constructor_004, TestSize.Level0)
{
    std::set<uint32_t> validtags = { 1, 2, 3 };

    std::vector<uint32_t> tags = { 1 };
    std::vector<uint32_t> sizes = { 1 };
    uint32_t tlvsize = 0;
    uint8_t *tlv = this->BuildTlv(tags, sizes, 0xAB, tlvsize);
    ASSERT_NE(tlv, nullptr);

    TlvBuf tb(tlv, tlvsize, validtags);
    ASSERT_EQ(tb.GetBufSize(), tlvsize);

    delete[] tlv;
}

HWTEST_F(HdcTLVTest, TlvBuf_Constructor_005, TestSize.Level0)
{
    std::set<uint32_t> validtags = { 1, 2, 3 };

    std::vector<uint32_t> tags = { 1 };
    std::vector<uint32_t> sizes = { TLV_VALUE_MAX_LEN + 1 };
    uint32_t tlvsize = 0;
    uint8_t *tlv = this->BuildTlv(tags, sizes, 0xAB, tlvsize);
    ASSERT_NE(tlv, nullptr);

    TlvBuf tb(tlv, tlvsize, validtags);
    ASSERT_EQ(tb.GetBufSize(), 0);

    delete[] tlv;
}

HWTEST_F(HdcTLVTest, TlvBuf_Clear_001, TestSize.Level0)
{
    std::vector<uint32_t> tags = { 1 };
    std::vector<uint32_t> sizes = { 1 };
    uint32_t tlvsize = 0;
    uint8_t *tlv = this->BuildTlv(tags, sizes, 0xAB, tlvsize);
    ASSERT_NE(tlv, nullptr);

    TlvBuf tb(tlv, tlvsize);
    ASSERT_EQ(tb.GetBufSize(), tlvsize);
    tb.Clear();
    ASSERT_EQ(tb.GetBufSize(), 0);

    delete[] tlv;
}

HWTEST_F(HdcTLVTest, TlvBuf_Append_001, TestSize.Level0)
{
    uint8_t val[10] = { 0xAC };
    TlvBuf tb;
    ASSERT_EQ(tb.GetBufSize(), 0);

    ASSERT_EQ(tb.Append(1, sizeof(val), val), true);
    ASSERT_EQ(tb.GetBufSize(), TlvSize(sizeof(val)));

    // duplicate, append failed, nothing changed
    ASSERT_EQ(tb.Append(1, sizeof(val), val), false);
    ASSERT_EQ(tb.GetBufSize(), TlvSize(sizeof(val)));

    ASSERT_EQ(tb.Append(2, sizeof(val), val), true);
    ASSERT_EQ(tb.GetBufSize(), TlvSize(sizeof(val)) * 2);
}

HWTEST_F(HdcTLVTest, TlvBuf_Append_002, TestSize.Level0)
{
    uint32_t tag = 1;
    string val = "hello world!";
    TlvBuf tb;
    ASSERT_EQ(tb.GetBufSize(), 0);

    ASSERT_EQ(tb.Append(tag, val), true);
    ASSERT_EQ(tb.GetBufSize(), TlvSize(val.size()));
}

HWTEST_F(HdcTLVTest, TlvBuf_Append_003, TestSize.Level0)
{
    TlvBuf tb;

    ASSERT_EQ(tb.Append(1, 0, nullptr), false);
    ASSERT_EQ(tb.GetBufSize(), 0);

    ASSERT_EQ(tb.Append(1, TLV_VALUE_MAX_LEN + 1, nullptr), false);
    ASSERT_EQ(tb.GetBufSize(), 0);

    ASSERT_EQ(tb.Append(1, TLV_VALUE_MAX_LEN, nullptr), false);
    ASSERT_EQ(tb.GetBufSize(), 0);
}

HWTEST_F(HdcTLVTest, TlvBuf_CopyToBuf_001, TestSize.Level0)
{
    uint32_t tag = 1;
    const uint32_t len = 10;
    uint8_t val[len] = { 0xAC };
    TlvBuf tb;
    ASSERT_EQ(tb.GetBufSize(), 0);

    ASSERT_EQ(tb.Append(tag, len, val), true);
    uint32_t size = tb.GetBufSize();
    ASSERT_EQ(size, TlvSize(len));

    uint8_t *buf = new (std::nothrow) uint8_t[size];
    ASSERT_EQ(tb.CopyToBuf(buf, size - 1), false);
    ASSERT_EQ(tb.CopyToBuf(nullptr, size), false);

    ASSERT_EQ(tb.CopyToBuf(buf, size), true);
    ASSERT_EQ(*(uint32_t *)buf, tag);
    ASSERT_EQ(*(uint32_t *)(buf + sizeof(tag)), len);
    ASSERT_EQ(memcmp(buf + TLV_HEAD_SIZE, val, len), 0);
}

HWTEST_F(HdcTLVTest, TlvBuf_FindTlv_001, TestSize.Level0)
{
    uint32_t tag = 1;
    const uint32_t len = 10;
    uint8_t val[len] = { 0xAC };
    TlvBuf tb;
    ASSERT_EQ(tb.GetBufSize(), 0);

    ASSERT_EQ(tb.Append(tag, len, val), true);
    ASSERT_EQ(tb.GetBufSize(), TlvSize(len));

    uint32_t ftag = 2;
    uint32_t flen = 1;
    uint8_t *fval = nullptr;
    ASSERT_EQ(tb.FindTlv(ftag, flen, fval), false);
    ASSERT_EQ(flen, 1);
    ASSERT_EQ(fval, nullptr);
    ftag = 1;
    ASSERT_EQ(tb.FindTlv(ftag, flen, fval), true);
    ASSERT_EQ(flen, len);
    ASSERT_EQ(memcmp(fval, val, len), 0);

    delete[] fval;
}

HWTEST_F(HdcTLVTest, TlvBuf_FindTlv_002, TestSize.Level0)
{
    uint32_t tag = 1;
    string val = "hello world!";
    TlvBuf tb;
    ASSERT_EQ(tb.GetBufSize(), 0);

    ASSERT_EQ(tb.Append(tag, val), true);
    ASSERT_EQ(tb.GetBufSize(), TlvSize(val.size()));

    string fval;
    ASSERT_EQ(tb.FindTlv(tag, fval), true);
    ASSERT_EQ(val, fval);
}

HWTEST_F(HdcTLVTest, TlvBuf_ContainInvalidTag_001, TestSize.Level0)
{
    std::set<uint32_t> validtags = { 1, 2, 3 };
    std::vector<uint32_t> tags = { 1, 2, 3 };
    std::vector<uint32_t> sizes = { 4, 4, 4 };
    uint32_t tlvsize = 0;
    uint8_t *tlv = this->BuildTlv(tags, sizes, 0xAB, tlvsize);
    ASSERT_NE(tlv, nullptr);
    TlvBuf tb(tlv, tlvsize, validtags);
    ASSERT_EQ(tb.GetBufSize(), tlvsize);

    ASSERT_EQ(tb.ContainInvalidTag(), false);

    uint32_t tag = 5;
    const uint32_t len = 10;
    uint8_t val[len] = { 0xAC };
    ASSERT_EQ(tb.Append(tag, len, val), true);
    ASSERT_EQ(tb.ContainInvalidTag(), true);

    delete[] tlv;
}

HWTEST_F(HdcTLVTest, TlvBuf_ContainInvalidTag_002, TestSize.Level0)
{
    std::set<uint32_t> validtags = { 1, 2 };
    std::vector<uint32_t> tags = { 1, 2, 3 };
    std::vector<uint32_t> sizes = { 4, 4, 4 };
    uint32_t tlvsize = 0;
    uint8_t *tlv = this->BuildTlv(tags, sizes, 0xAB, tlvsize);
    ASSERT_NE(tlv, nullptr);
    TlvBuf tb(tlv, tlvsize, validtags);
    ASSERT_EQ(tb.GetBufSize(), tlvsize);

    ASSERT_EQ(tb.ContainInvalidTag(), true);

    delete[] tlv;
}

HWTEST_F(HdcTLVTest, TlvBuf_Display_001, TestSize.Level0)
{
    std::vector<uint32_t> tags = { 1 };
    std::vector<uint32_t> sizes = { 1 };
    uint32_t tlvsize = 0;
    uint8_t *tlv = this->BuildTlv(tags, sizes, 0xAB, tlvsize);
    ASSERT_NE(tlv, nullptr);

    TlvBuf tb(tlv, tlvsize);
    ASSERT_EQ(tb.GetBufSize(), tlvsize);
    tb.Display();

    delete[] tlv;
}

} // namespace Hdc
