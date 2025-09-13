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
#include "header_test.h"
#include "header.h"

using namespace testing::ext;

namespace Hdc {
void HdcHeaderTest::SetUpTestCase() {}
void HdcHeaderTest::TearDownTestCase() {}
void HdcHeaderTest::SetUp() {}
void HdcHeaderTest::TearDown() {}
std::string HdcHeaderTest::DecimalToOctalString(size_t decimalNumber, int length)
{
    std::ostringstream oss;
    oss << std::oct << std::setw(length) << std::setfill('0') << decimalNumber;
    return oss.str();
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_NormalCase, TestSize.Level0)
{
    EXPECT_EQ(DecimalToOctalString(10, 4), "0012");
    EXPECT_EQ(DecimalToOctalString(15, 3), "017");
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_ZeroInput, TestSize.Level0)
{
    EXPECT_EQ(DecimalToOctalString(0, 4), "0000");
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_MaxInput, TestSize.Level0)
{
    size_t maxValue = std::numeric_limits<size_t>::max();
    std::string result = DecimalToOctalString(maxValue, 20);
    EXPECT_FALSE(result.empty());
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_InvalidLength, TestSize.Level0)
{
    EXPECT_EQ(DecimalToOctalString(10, 0), "12");
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_EdgeCases, TestSize.Level0)
{
    EXPECT_EQ(DecimalToOctalString(1, 1), "1");
    EXPECT_EQ(DecimalToOctalString(8, 2), "10");
}

HWTEST_F(HdcHeaderTest, TestName_EmptyStr, TestSize.Level0)
{
    Header header;
    EXPECT_EQ(header.Name(), "");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_ShortName, TestSize.Level0)
{
    Header header;
    std::string fileName = "short_name.txt";
    EXPECT_TRUE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "short_name.txt");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_LongName, TestSize.Level0)
{
    Header header;
    std::string fileName(HEADER_MAX_FILE_LEN + 1, 'a');
    EXPECT_FALSE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_EdgeCase, TestSize.Level0)
{
    Header header;
    std::string fileName(HEADER_MAX_FILE_LEN, 'a');
    EXPECT_FALSE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_EmptyName, TestSize.Level0)
{
    Header header;
    std::string fileName = "";
    EXPECT_TRUE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "");
}

HWTEST_F(HdcHeaderTest, TestUpdataSize_Zero, TestSize.Level0)
{
    Header header;
    header.UpdataSize(0);
    EXPECT_EQ(header.Size(), 0);
}

HWTEST_F(HdcHeaderTest, TestUpdataSize, TestSize.Level0)
{
    Header header;
    header.UpdataSize(10);
    EXPECT_EQ(header.Size(), 10);
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_INVALID, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::INVALID);
    EXPECT_EQ(header.FileType(), 0);
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_ORDINARYFILE, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::ORDINARYFILE);
    EXPECT_EQ(header.FileType(), '0');
}

HWTEST_F(HdcHeaderTest, TestIsInvalid_INVALID, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::INVALID);
    EXPECT_TRUE(header.IsInvalid());
}

HWTEST_F(HdcHeaderTest, TestIsInvalid_ORDINARYFILE, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::ORDINARYFILE);
    EXPECT_FALSE(header.IsInvalid());
}

HWTEST_F(HdcHeaderTest, TestUpdataCheckSum, TestSize.Level0)
{
    Header header;
    header.UpdataCheckSum();
    EXPECT_EQ(header.chksum[0], '0');
}
}
