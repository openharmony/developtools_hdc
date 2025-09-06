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
#include "credential_base_test.h"
#include "credential_base.h"

using namespace testing::ext;

namespace HdcCredentialBase {
void HdcCredentialBaseTest::SetUpTestCase() {}
void HdcCredentialBaseTest::TearDownTestCase() {}
void HdcCredentialBaseTest::SetUp() {}
void HdcCredentialBaseTest::TearDown() {}

HWTEST_F(HdcCredentialBaseTest, TestCreatePathWithMode_NullDir, TestSize.Level1)
{
    EXPECT_FALSE(CreatePathWithMode("", MODE));
}

HWTEST_F(HdcCredentialBaseTest, TestCreatePathWithMode_MultiLevelDir, TestSize.Level1)
{
    // not support.
    EXPECT_FALSE(CreatePathWithMode("./1/2/3", MODE));

    EXPECT_TRUE(CreatePathWithMode("./1", MODE));
    EXPECT_TRUE(CreatePathWithMode("./1/2", MODE));
    EXPECT_TRUE(CreatePathWithMode("./1/2/3", MODE));

    // remove directory after test, avoid next test fail.
    EXPECT_EQ(RemovePath("./1"), 0);
}

HWTEST_F(HdcCredentialBaseTest, TestCreatePathWithMode_SimpleLevelDir, TestSize.Level1)
{
    EXPECT_TRUE(CreatePathWithMode("./11", MODE));
    // remove directory after test, avoid next test fail.
    EXPECT_EQ(RemoveDir("./11"), 0);
}

HWTEST_F(HdcCredentialBaseTest, TestCreatePathWithMode_CreateExistsPath, TestSize.Level1)
{
    EXPECT_TRUE(CreatePathWithMode("./111", MODE));
    // create same path again.
    EXPECT_FALSE(CreatePathWithMode("./111", MODE));

    // remove directory after test, avoid next test fail.
    EXPECT_EQ(RemoveDir("./111"), 0);
}

HWTEST_F(HdcCredentialBaseTest, TestRemovePath_NullPath, TestSize.Level1)
{
    EXPECT_EQ(RemovePath(""), -1);
}

HWTEST_F(HdcCredentialBaseTest, TestRemovePath_NotExistsPath, TestSize.Level1)
{
    EXPECT_EQ(RemovePath("./not_exist_dir"), -1);
}

HWTEST_F(HdcCredentialBaseTest, TestRemovePath_SimpleLavelPath, TestSize.Level1)
{
    EXPECT_TRUE(CreatePathWithMode("./1111", MODE));
    // remove directory after test, avoid next test fail.
    EXPECT_EQ(RemovePath("./1111"), 0);
}

HWTEST_F(HdcCredentialBaseTest, TestRemovePath_MultiLavelPath, TestSize.Level1)
{
    EXPECT_TRUE(CreatePathWithMode("./11111", MODE));
    EXPECT_TRUE(CreatePathWithMode("./11111/22222", MODE));
    EXPECT_TRUE(CreatePathWithMode("./11111/22222/33333", MODE));
    // remove directory after test, avoid next test fail.
    EXPECT_EQ(RemovePath("./11111"), 0);
}

HWTEST_F(HdcCredentialBaseTest, TestStringFormat_NullStr, TestSize.Level1)
{
    EXPECT_EQ(StringFormat(""), "");
}

HWTEST_F(HdcCredentialBaseTest, TestStringFormat_NormalStr, TestSize.Level1)
{
    EXPECT_EQ(StringFormat("%s", "test"), "test");
    EXPECT_EQ(StringFormat("%d + %d = %d", 1, 2, 3), "1 + 2 = 3");
}

HWTEST_F(HdcCredentialBaseTest, TestStringFormat_HexStr, TestSize.Level1)
{
    EXPECT_EQ(StringFormat("%x", 255), "ff");
}

HWTEST_F(HdcCredentialBaseTest, TestStringFormat_OctalStr, TestSize.Level1)
{
    EXPECT_EQ(StringFormat("%o", 255), "377");
}

HWTEST_F(HdcCredentialBaseTest, TestStringFormat_PointerStr, TestSize.Level1)
{
        EXPECT_EQ(StringFormat("%p", (void*)0x1234), "0x1234");
}

HWTEST_F(HdcCredentialBaseTest, TestStringFormat_MultiArgsStr, TestSize.Level1)
{
    EXPECT_EQ(StringFormat("%s%c", "test", 'c'), "testc");
    EXPECT_EQ(StringFormat("%s %d + %d = %d", "test", 1, 2, 3), "test 1 + 2 = 3");
    EXPECT_EQ(StringFormat("%c+%o+%p+%s", 'c', 255, (void*)0x1234, "test"), "c+377+0x1234+test");
}

HWTEST_F(HdcCredentialBaseTest, TestIsUserDir_NotNumber, TestSize.Level1)
{
    EXPECT_FALSE(IsUserDir("aaa"));
}

HWTEST_F(HdcCredentialBaseTest, TestIsUserDir_InvalidNumber, TestSize.Level1)
{
    EXPECT_FALSE(IsUserDir("99"));
    EXPECT_FALSE(IsUserDir("10737"));
}

HWTEST_F(HdcCredentialBaseTest, TestIsUserDir_ValidNumber, TestSize.Level1)
{
    EXPECT_TRUE(IsUserDir("102"));
    EXPECT_TRUE(IsUserDir("10000"));
}

HWTEST_F(HdcCredentialBaseTest, TestIsUserDir_BoundaryNumber, TestSize.Level1)
{
    EXPECT_TRUE(IsUserDir("100"));
    EXPECT_TRUE(IsUserDir("10736"));
}

HWTEST_F(HdcCredentialBaseTest, TestSubstract_Normal, TestSize.Level1)
{
    std::vector<std::string> a = {"1", "2", "3"};
    std::vector<std::string> b = {"2", "3", "4"};
    std::vector<std::string> diffA_B = Substract<std::string>(a, b);
    EXPECT_EQ(diffA_B.size(), 1);
    EXPECT_EQ(diffA_B[0], "1");

    std::vector<std::string> diffB_A = Substract<std::string>(b, a);
    EXPECT_EQ(diffB_A.size(), 1);
    EXPECT_EQ(diffB_A[0], "4");
}

HWTEST_F(HdcCredentialBaseTest, TestRemoveDir_NullPath, TestSize.Level1)
{
    EXPECT_EQ(RemoveDir(""), -1);
}

HWTEST_F(HdcCredentialBaseTest, TestRemoveDir_NotExistsPath, TestSize.Level1)
{
    EXPECT_EQ(RemoveDir("./not_exist_dir"), -1);
}

HWTEST_F(HdcCredentialBaseTest, TestRemoveDir_SimpleLevelPath, TestSize.Level1)
{
    EXPECT_TRUE(CreatePathWithMode("./1111", MODE));
    EXPECT_EQ(RemoveDir("./1111"), 0);
}

HWTEST_F(HdcCredentialBaseTest, TestRemoveDir_MultiLevelPath, TestSize.Level1)
{
    EXPECT_TRUE(CreatePathWithMode("./11111", MODE));
    EXPECT_TRUE(CreatePathWithMode("./11111/22222", MODE));
    EXPECT_TRUE(CreatePathWithMode("./11111/22222/33333", MODE));
    EXPECT_EQ(RemoveDir("./11111"), 0);
}

}