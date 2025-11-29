/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <securec.h>
#include "serial_struct_define.h"
#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing::ext;
using namespace testing;

namespace Hdc {

class ZigzagTest : public Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void ZigzagTest::SetUpTestCase() {}
void ZigzagTest::TearDownTestCase() {}
void ZigzagTest::SetUp() {}
void ZigzagTest::TearDown() {}

// 测试 32 位 Zigzag 编码
HWTEST_F(ZigzagTest, TestMakeZigzag32_PositiveNumbers, TestSize.Level0)
{
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(0), 0u);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(1), 2u);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(2), 4u);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(100), 200u);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(1000), 2000u);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(std::numeric_limits<int32_t>::max()),
                static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) * 2);
}

HWTEST_F(ZigzagTest, TestMakeZigzag32_NegativeNumbers, TestSize.Level0)
{
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(-1), 1u);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(-2), 3u);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(-100), 199u);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(-1000), 1999u);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(std::numeric_limits<int32_t>::min()),
                static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) * 2 + 1);
}

// 测试64位Zigzag编码
HWTEST_F(ZigzagTest, TestMakeZigzag64_PositiveNumbers, TestSize.Level0)
{
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(0LL), 0ULL);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(1LL), 2ULL);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(2LL), 4ULL);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(100LL), 200ULL);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(1000LL), 2000ULL);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(std::numeric_limits<int64_t>::max()),
                static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) * 2);
}

HWTEST_F(ZigzagTest, TestMakeZigzag64_NegativeNumbers, TestSize.Level0)
{
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(-1LL), 1ULL);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(-2LL), 3ULL);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(-100LL), 199ULL);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(-1000LL), 1999ULL);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(std::numeric_limits<int64_t>::min()),
                static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) * 2 + 1);
}

// 测试32位Zigzag解码
HWTEST_F(ZigzagTest, TestReadZigzag32_PositiveNumbers, TestSize.Level0)
{
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(0u), 0);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(2u), 1);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(4u), 2);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(200u), 100);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(2000u), 1000);
}

HWTEST_F(ZigzagTest, TestReadZigzag32_NegativeNumbers, TestSize.Level0)
{
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(1u), -1);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(3u), -2);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(199u), -100);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(1999u), -1000);
}

// 测试64位Zigzag解码
HWTEST_F(ZigzagTest, TestReadZigzag64_PositiveNumbers, TestSize.Level0)
{
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(0ULL), 0LL);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(2ULL), 1LL);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(4ULL), 2LL);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(200ULL), 100LL);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(2000ULL), 1000LL);
}

HWTEST_F(ZigzagTest, TestReadZigzag64_NegativeNumbers, TestSize.Level0)
{
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(1ULL), -1LL);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(3ULL), -2LL);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(199ULL), -100LL);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(1999ULL), -1000LL);
}

// 测试编解码的往返一致性
HWTEST_F(ZigzagTest, TestRoundTrip32, TestSize.Level0)
{
    int32_t test_values[] = {0, 1, -1, 2, -2, 100, -100, 1000, -1000,
                                std::numeric_limits<int32_t>::max(),
                                std::numeric_limits<int32_t>::min()};

    for (auto value : test_values)
    {
        uint32_t encoded = SerialStruct::SerialDetail::MakeZigzagValue(value);
        int32_t decoded = SerialStruct::SerialDetail::ReadZigzagValue(encoded);
        EXPECT_EQ(decoded, value) << "Failed for value: " << value;
    }
}

HWTEST_F(ZigzagTest, TestRoundTrip64, TestSize.Level0)
{
    int64_t test_values[] = {0, 1, -1, 2, -2, 100, -100, 1000, -1000,
                                std::numeric_limits<int64_t>::max(),
                                std::numeric_limits<int64_t>::min()};

    for (auto value : test_values)
    {
        uint64_t encoded = SerialStruct::SerialDetail::MakeZigzagValue(value);
        int64_t decoded = SerialStruct::SerialDetail::ReadZigzagValue(encoded);
        EXPECT_EQ(decoded, value) << "Failed for value: " << value;
    }
}

// 边界值测试
HWTEST_F(ZigzagTest, TestBoundaryValues32, TestSize.Level0)
{
    // 测试最大值和最小值
    int32_t min_val = std::numeric_limits<int32_t>::min();
    int32_t max_val = std::numeric_limits<int32_t>::max();

    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(SerialStruct::SerialDetail::MakeZigzagValue(min_val)),
              min_val);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(SerialStruct::SerialDetail::MakeZigzagValue(max_val)),
              max_val);

    // 测试编码后的边界值
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(min_val), 0xFFFFFFFFu);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(max_val), 0xFFFFFFFEu);
}

HWTEST_F(ZigzagTest, TestBoundaryValues64, TestSize.Level0)
{
    // 测试最大值和最小值
    int64_t min_val = std::numeric_limits<int64_t>::min();
    int64_t max_val = std::numeric_limits<int64_t>::max();

    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(SerialStruct::SerialDetail::MakeZigzagValue(min_val)),
              min_val);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(SerialStruct::SerialDetail::MakeZigzagValue(max_val)),
              max_val);

    // 测试编码后的边界值
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(min_val), 0xFFFFFFFFFFFFFFFFULL);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(max_val), 0xFFFFFFFFFFFFFFFEULL);
}

// 特殊值测试
HWTEST_F(ZigzagTest, TestSpecialCases, TestSize.Level0)
{
    // 测试0
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(0), 0u);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(0u), 0);

    // 测试-1和1
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(-1), 1u);
    EXPECT_EQ(SerialStruct::SerialDetail::MakeZigzagValue(1), 2u);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(1u), -1);
    EXPECT_EQ(SerialStruct::SerialDetail::ReadZigzagValue(2u), 1);
}

}