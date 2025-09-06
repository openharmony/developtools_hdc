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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base.h"
#include "define.h"

using namespace testing::ext;
using namespace testing;

namespace Hdc {
class BaseTest : public ::testing::Test {
    private:
        static void SetUpTestCase(void);
        static void TearDownTestCase(void);
        void SetUp();
        void TearDown();
};

void BaseTest::SetUpTestCase() {}
void BaseTest::TearDownTestCase() {}
void BaseTest::SetUp() {}
void BaseTest::TearDown() {}
    
HWTEST_F(BaseTest, UpdateCmdLogSwitch_12, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "12", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(BaseTest, UpdateCmdLogSwitch_, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(BaseTest, UpdateCmdLogSwitch_1, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "1", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_TRUE(Base::GetCmdLogSwitch());
}

HWTEST_F(BaseTest, UpdateCmdLogSwitch_0, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "0", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(BaseTest, UpdateCmdLogSwitch_a, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "a", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(BaseTest, ReplaceAll_EmptyStr, TestSize.Level0) {
    std::string src = "";
    EXPECT_EQ(Base::ReplaceAll(src, "a", "b"), "");
}

HWTEST_F(BaseTest, ReplaceAll_NotMatch, TestSize.Level0) {
    std::string src = "ccc";
    EXPECT_EQ(Base::ReplaceAll(src, "a", "b"), "ccc");
}

HWTEST_F(BaseTest, ReplaceAll_Match_1, TestSize.Level0) {
    std::string src = "ccc";
    EXPECT_EQ(Base::ReplaceAll(src, "c", "b"), "bbb");
}

HWTEST_F(BaseTest, ReplaceAll_Match_2, TestSize.Level0) {
    std::string src = "abcde";
    EXPECT_EQ(Base::ReplaceAll(src, "c", "b"), "abbde");
}

HWTEST_F(BaseTest, CanonicalizeSpecPath_EmptyPath, TestSize.Level0) {
    std::string src = "";
    std::string res = Base::CanonicalizeSpecPath(src);
    EXPECT_TRUE(res.empty());
}

HWTEST_F(BaseTest, CanonicalizeSpecPath_NotExistPath_1, TestSize.Level0) {
    std::string src = "/notexist-path/xxx";
    std::string res = Base::CanonicalizeSpecPath(src);
    EXPECT_TRUE(res.empty());
}

HWTEST_F(BaseTest, CanonicalizeSpecPath_NotExistPath_2, TestSize.Level0) {
    std::string src = "/a/b/c";
    std::string res = Base::CanonicalizeSpecPath(src);
    EXPECT_TRUE(res.empty());
}

HWTEST_F(BaseTest, CanonicalizeSpecPath_RealPath, TestSize.Level0) {
    std::string src = "/data/local/tmp";
    std::string res = Base::CanonicalizeSpecPath(src);
    EXPECT_FALSE(res.empty());
    EXPECT_EQ(res, "/data/local/tmp");
}

HWTEST_F(BaseTest, CanonicalizeSpecPath_RealRelativePath_1, TestSize.Level0) {
    std::string src = "/data/local/tmp/../";
    std::string res = Base::CanonicalizeSpecPath(src);
    EXPECT_FALSE(res.empty());
    EXPECT_EQ(res, "/data/local");
}

HWTEST_F(BaseTest, CanonicalizeSpecPath_RealRelativePath_2, TestSize.Level0) {
    std::string src = "/data/local/tmp/../../";
    std::string res = Base::CanonicalizeSpecPath(src);
    EXPECT_FALSE(res.empty());
    EXPECT_EQ(res, "/data");
}

HWTEST_F(BaseTest, UnicodeToUtf8_EmptyStr, TestSize.Level0) {
    const char *src = "";
    EXPECT_TRUE(Base::UnicodeToUtf8(src, true).empty());
}

HWTEST_F(BaseTest, UnicodeToUtf8_ValidStr, TestSize.Level0) {
    const char *src = "abc";
    std::string res = Base::UnicodeToUtf8(src, true);
    EXPECT_FALSE(res.empty());
    EXPECT_EQ(res, "abc");
}

HWTEST_F(BaseTest, CalcCheckSum_EmptyInput, TestSize.Level0) {
    uint8_t data[] = {};
    EXPECT_EQ(Base::CalcCheckSum(data, 0), 0);
}

HWTEST_F(BaseTest, CalcCheckSum_SingleInput, TestSize.Level0) {
    uint8_t data[] = {0x01};
    EXPECT_EQ(Base::CalcCheckSum(data, 1), 0x01);
}

HWTEST_F(BaseTest, CalcCheckSum_MultiInput, TestSize.Level0) {
    uint8_t data[] = {0x01, 0x02, 0x03};
    EXPECT_EQ(Base::CalcCheckSum(data, 3), 0x06);
}

HWTEST_F(BaseTest, UpdateTmpDirCache, TestSize.Level0) {
    Base::UpdateTmpDirCache();
    EXPECT_FALSE(Base::GetTmpDir().empty());
    EXPECT_EQ(Base::GetTmpDir(), "/data/local/tmp/");
}

HWTEST_F(BaseTest, IsRoot, TestSize.Level0) {
    // run ut shoud be root
    EXPECT_TRUE(Base::IsRoot());
}

HWTEST_F(BaseTest, IsAbsolutePath_ExptyPath, TestSize.Level0) {
    std::string src = "";
    EXPECT_FALSE(Base::IsAbsolutePath(src));
}

HWTEST_F(BaseTest, IsAbsolutePath_AbsolutePath, TestSize.Level0) {
    std::string src = "/a/b/c";
    EXPECT_TRUE(Base::IsAbsolutePath(src));
}

HWTEST_F(BaseTest, IsAbsolutePath_RelativePath_1, TestSize.Level0) {
    std::string src = "../a/b/c";
    EXPECT_FALSE(Base::IsAbsolutePath(src));
}

HWTEST_F(BaseTest, IsAbsolutePath_RelativePath_2, TestSize.Level0) {
    std::string src = "a/b/c";
    EXPECT_FALSE(Base::IsAbsolutePath(src));
}

HWTEST_F(BaseTest, CloseFd_TEST_1, TestSize.Level0) {
    int fd = 0;
    EXPECT_EQ(Base::CloseFd(fd), 0);
}

HWTEST_F(BaseTest, CloseFd_TEST_2, TestSize.Level0) {
    int fd = -1;
    EXPECT_EQ(Base::CloseFd(fd), 0);
}

HWTEST_F(BaseTest, IsValidIpv4_EmptyStr, TestSize.Level0) {
    std::string ip = "";
    EXPECT_FALSE(Base::IsValidIpv4(ip));
}

HWTEST_F(BaseTest, IsValidIpv4_InvalidIP, TestSize.Level0) {
    std::string ip = "1.2.";
    EXPECT_FALSE(Base::IsValidIpv4(ip));
}

HWTEST_F(BaseTest, IsValidIpv4_ValidIP_1, TestSize.Level0) {
    std::string ip = "127.0.0.1";
    EXPECT_TRUE(Base::IsValidIpv4(ip));
}

HWTEST_F(BaseTest, IsValidIpv4_ValidIP_2, TestSize.Level0) {
    std::string ip = "0.0.0.0";
    EXPECT_TRUE(Base::IsValidIpv4(ip));
}

HWTEST_F(BaseTest, IsValidIpv4_IPv6, TestSize.Level0) {
    std::string ip = "2000:0:0:0:0:0:0:1";
    EXPECT_FALSE(Base::IsValidIpv4(ip));
}

HWTEST_F(BaseTest, ShellCmdTrim_EmptyStr, TestSize.Level0) {
    std::string cmd = "";
    EXPECT_EQ(Base::ShellCmdTrim(cmd), "");
}

HWTEST_F(BaseTest, ShellCmdTrim_TooShort, TestSize.Level0) {
    std::string cmd = "\"";
    EXPECT_EQ(Base::ShellCmdTrim(cmd), "\"");
}

HWTEST_F(BaseTest, ShellCmdTrim_Trim, TestSize.Level0) {
    std::string cmd = "  \"  \"  ";
    EXPECT_EQ(Base::ShellCmdTrim(cmd), "  ");
}

HWTEST_F(BaseTest, TrimSubString_EmptyStr, TestSize.Level0) {
    std::string str = "";
    std::string sub = "xxx";
    Base::TrimSubString(str, sub);
    EXPECT_EQ(str, "");
}

HWTEST_F(BaseTest, TrimSubString_NotMatch, TestSize.Level0) {
    std::string str = "aaaaa";
    std::string sub = "b";
    Base::TrimSubString(str, sub);
    EXPECT_EQ(str, "aaaaa");
}

HWTEST_F(BaseTest, TrimSubString_Match, TestSize.Level0) {
    std::string str = "aaaaa";
    std::string sub = "a";
    Base::TrimSubString(str, sub);
    EXPECT_EQ(str, "");
}

HWTEST_F(BaseTest, TlvAppend_EmptyStr, TestSize.Level0) {
    std::string str = "";
    EXPECT_FALSE(Base::TlvAppend(str, "", ""));
}

HWTEST_F(BaseTest, TlvAppend_ShortStr, TestSize.Level0) {
    std::string tlv;
    std::string tag = "12345";
    std::string val = "";
    EXPECT_TRUE(Base::TlvAppend(tlv, tag, val));
    EXPECT_EQ(tlv.size(), 32);
    EXPECT_EQ(tlv, "12345           0               ");
}

HWTEST_F(BaseTest, TlvAppend_ShortStr_2, TestSize.Level0) {
    std::string tlv;
    std::string tag = "12345";
    std::string val = "123";
    EXPECT_TRUE(Base::TlvAppend(tlv, tag, val));
    EXPECT_EQ(tlv.size(), 35);
    EXPECT_EQ(tlv, "12345           3               123");
}

HWTEST_F(BaseTest, TlvAppend_ShortStr_3, TestSize.Level0) {
    std::string tlv;
    std::string tag = "12345";
    std::string val = "1234";
    EXPECT_TRUE(Base::TlvAppend(tlv, tag, val));
    EXPECT_EQ(tlv.size(), 36);
    EXPECT_EQ(tlv, "12345           4               1234");
}

HWTEST_F(BaseTest, TlvToStringMap_TooShort_1, TestSize.Level0) {
    std::map<string, string> mp;
    EXPECT_FALSE(Base::TlvToStringMap("", mp));
}

HWTEST_F(BaseTest, TlvToStringMap_TooShort_2, TestSize.Level0) {
    std::map<string, string> mp;
    EXPECT_FALSE(Base::TlvToStringMap("", mp));
}

HWTEST_F(BaseTest, TlvToStringMap_TEST, TestSize.Level0) {
    std::map<string, string> mp;
    EXPECT_TRUE(Base::TlvToStringMap("12345           3               123", mp));
    for (auto it = mp.begin(); it != mp.end(); ++it) {
        EXPECT_EQ(it->first, "12345");
        EXPECT_EQ(it->second, "123");
    }
}

HWTEST_F(BaseTest, CheckBundleName_EmptyStr, TestSize.Level0) {
    std::string bundleName = "";
    EXPECT_FALSE(Base::CheckBundleName(bundleName));
}

HWTEST_F(BaseTest, CheckBundleName_InvalidLength, TestSize.Level0) {
    // min_size=7
    std::string bundleName = "123456";
    EXPECT_FALSE(Base::CheckBundleName(bundleName));
}

HWTEST_F(BaseTest, CheckBundleName_InvalidLength_2, TestSize.Level0) {
    // max_size=128
    size_t size = 129;
    std::string bundleName(size, 'a');
    EXPECT_FALSE(Base::CheckBundleName(bundleName));
}

HWTEST_F(BaseTest, CheckBundleName_InvalidLength_3, TestSize.Level0) {
    // 0-9,a-Z,_,.组成的字符串
    std::string bundleName = "<KEY>";
    EXPECT_FALSE(Base::CheckBundleName(bundleName));
}

HWTEST_F(BaseTest, CheckBundleName_Valid, TestSize.Level0) {
    // 0-9,a-Z,_,.组成的字符串
    std::string bundleName = "com.xxx.xxx.xxx_xxx.xxx";
    EXPECT_TRUE(Base::CheckBundleName(bundleName));
}

HWTEST_F(BaseTest, CanPrintCmd_FORBIDEN_1, TestSize.Level0) {
    EXPECT_FALSE(Base::CanPrintCmd(CMD_APP_DATA));
}

HWTEST_F(BaseTest, CanPrintCmd_FORBIDEN_2, TestSize.Level0) {
    EXPECT_FALSE(Base::CanPrintCmd(CMD_FILE_DATA));
}

HWTEST_F(BaseTest, CanPrintCmd_FORBIDEN_3, TestSize.Level0) {
    EXPECT_FALSE(Base::CanPrintCmd(CMD_FORWARD_DATA));
}

HWTEST_F(BaseTest, CanPrintCmd_FORBIDEN_4, TestSize.Level0) {
    EXPECT_FALSE(Base::CanPrintCmd(CMD_SHELL_DATA));
}

HWTEST_F(BaseTest, CanPrintCmd_FORBIDEN_5, TestSize.Level0) {
    EXPECT_FALSE(Base::CanPrintCmd(CMD_UNITY_BUGREPORT_DATA));
}

HWTEST_F(BaseTest, CanPrintCmd_ALLOW, TestSize.Level0) {
    EXPECT_TRUE(Base::CanPrintCmd(CMD_UNITY_BUGREPORT_INIT));
}

} // namespace Hdc