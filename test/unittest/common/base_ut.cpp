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
#include <fstream>

#include "base.h"
#include "define.h"

using namespace testing::ext;
using namespace testing;

namespace Hdc {
class BaseTest : public ::testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
private:
    char **argv = nullptr;
    int slotIndex = 0;
};

void BaseTest::SetUpTestCase() {}
void BaseTest::TearDownTestCase() {}
void BaseTest::SetUp() {}
void BaseTest::TearDown() {}

const Base::HdcFeatureSet& featureSet = Base::GetSupportFeature();

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
// 验证返回的特性集合不为空
HWTEST_F(BaseTest, FeatureSet_ReturnsNonEmpty, TestSize.Level0) {
    EXPECT_FALSE(featureSet.empty());
    EXPECT_EQ(featureSet.size(), 2);
}

// 验证多次调用返回相同引用
HWTEST_F(BaseTest, FeatureSet_ReturnsSameReferenceOnMultipleCalls, TestSize.Level0) {
    const Base::HdcFeatureSet& firstCall = Base::GetSupportFeature();
    const Base::HdcFeatureSet& secondCall = Base::GetSupportFeature();
    
    EXPECT_EQ(&firstCall, &secondCall);
    EXPECT_EQ(&featureSet, &firstCall);
}

// 验证包含预期的特性
HWTEST_F(BaseTest, FeatureSet_ContainsExpectedFeatures, TestSize.Level0) {

    EXPECT_TRUE(std::find(featureSet.begin(), featureSet.end(), FEATURE_HEARTBEAT) != featureSet.end());
    EXPECT_TRUE(std::find(featureSet.begin(), featureSet.end(), FEATURE_ENCRYPT_TCP) != featureSet.end());
}

// 验证不包含非预期的特性
HWTEST_F(BaseTest, FeatureSet_DoesNotContainUnexpectedFeatures, TestSize.Level0) {
    EXPECT_FALSE(std::find(featureSet.begin(), featureSet.end(), "unknown") != featureSet.end());
    
    EXPECT_EQ(featureSet.size(), 2);
}

// 验证当输入为空容器时，函数返回空字符串
HWTEST_F(BaseTest, FeatureToStringTest_EmptyContainer, TestSize.Level0) {
    Base::HdcFeatureSet empty_feature;
    std::string result = Base::FeatureToString(empty_feature);
    EXPECT_EQ(result, "");
}

// 验证当输入只包含一个元素时，函数返回该元素且不包含逗号
HWTEST_F(BaseTest, FeatureToStringTest_SingleElement, TestSize.Level0) {
    Base::HdcFeatureSet single_feature = {"feature1"};
    std::string result = Base::FeatureToString(single_feature);
    EXPECT_EQ(result, "feature1");
}

// 验证当输入包含两个元素时，函数正确添加逗号分隔符
HWTEST_F(BaseTest, FeatureToStringTest_TwoElements, TestSize.Level0) {
    Base::HdcFeatureSet two_features = {"feature1", "feature2"};
    std::string result = Base::FeatureToString(two_features);
    EXPECT_EQ(result, "feature1,feature2");
}

// 验证当输入包含多个元素时，函数正确处理所有元素和分隔符
HWTEST_F(BaseTest, FeatureToStringTest_MultipleElements, TestSize.Level0) {
    Base::HdcFeatureSet multiple_features = {"feature1", "feature2", "feature3", "feature4"};
    std::string result = Base::FeatureToString(multiple_features);
    EXPECT_EQ(result, "feature1,feature2,feature3,feature4");
}

// 验证当容器包含空字符串时，函数正确处理
HWTEST_F(BaseTest, FeatureToStringTest_ElementsWithEmptyString, TestSize.Level0) {
    Base::HdcFeatureSet features_with_empty = {"feature1", "", "feature3"};
    std::string result = Base::FeatureToString(features_with_empty);
    EXPECT_EQ(result, "feature1,,feature3");
}

// 验证当元素包含逗号或其他特殊字符时，函数正确处理
HWTEST_F(BaseTest, FeatureToStringTest_ElementsWithSpecialCharacters, TestSize.Level0) {
    Base::HdcFeatureSet special_features = {"feature,1", "feature:2", "feature;3"};
    std::string result = Base::FeatureToString(special_features);
    EXPECT_EQ(result, "feature,1,feature:2,feature;3");
}

// 多个特征的字符串转换
HWTEST_F(BaseTest, StringToFeatureSet_NormalCase, TestSize.Level0) {
    Base::HdcFeatureSet features;
    std::string input = "feature1,feature2,feature3";
    
    Base::StringToFeatureSet(input, features);
    
    ASSERT_EQ(features.size(), 3);
    EXPECT_EQ(features[0], "feature1");
    EXPECT_EQ(features[1], "feature2");
    EXPECT_EQ(features[2], "feature3");
}

// 测试单个特征情况
HWTEST_F(BaseTest, StringToFeatureSet_SingleFeature, TestSize.Level0) {
    Base::HdcFeatureSet features;
    std::string input = "feature1";
    
    Base::StringToFeatureSet(input, features);
    
    ASSERT_EQ(features.size(), 1);
    EXPECT_EQ(features[0], "feature1");
}

// 测试空字符串情况
HWTEST_F(BaseTest, StringToFeatureSet_EmptyString, TestSize.Level0) {
    Base::HdcFeatureSet features;
    std::string input = "";
    
    Base::StringToFeatureSet(input, features);
    
    EXPECT_TRUE(features.empty());
}

//  测试包含空元素的情况
HWTEST_F(BaseTest, StringToFeatureSet_WithEmptyElements, TestSize.Level0) {
    Base::HdcFeatureSet features;
    std::string input = "feature1,,feature2";
    
    Base::StringToFeatureSet(input, features);
    
    ASSERT_EQ(features.size(), 2);
    EXPECT_EQ(features[0], "feature1");
    EXPECT_EQ(features[1], "feature2");
}

// 测试只有分隔符的情况
HWTEST_F(BaseTest, StringToFeatureSet_OnlyDelimiter, TestSize.Level0) {
    Base::HdcFeatureSet features;
    std::string input = ",";
    
    Base::StringToFeatureSet(input, features);
    
    ASSERT_EQ(features.size(), 0);
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

HWTEST_F(BaseTest, TestConnectKey2IPv4Port, TestSize.Level0) {
    const char* connectKey = "127.0.0.1:8710";
    char outIP[BUF_SIZE_TINY] = "";
    uint16_t outPort = 0;
    int result = Base::ConnectKey2IPPort(connectKey, outIP, &outPort, sizeof(outIP));

    EXPECT_EQ(result, RET_SUCCESS);
    EXPECT_EQ(std::string(outIP), "127.0.0.1");
    EXPECT_EQ(outPort, 8710);
}

HWTEST_F(BaseTest, TestConnectKey2IPv6Port, TestSize.Level0) {
    const char* connectKey = "::1:8710";
    char outIP[BUF_SIZE_TINY] = "";
    uint16_t outPort = 0;
    int result = Base::ConnectKey2IPPort(connectKey, outIP, &outPort, sizeof(outIP));

    EXPECT_EQ(result, RET_SUCCESS);
    EXPECT_EQ(std::string(outIP), "::1");
    EXPECT_EQ(outPort, 8710);
}

HWTEST_F(BaseTest, TestConnectKey2IPInvalid, TestSize.Level0) {
    const char* connectKey = "127.0.0.1";
    char outIP[BUF_SIZE_TINY] = "";
    uint16_t outPort = 0;

    int result = Base::ConnectKey2IPPort(connectKey, outIP, &outPort, sizeof(outIP));
    EXPECT_EQ(result, ERR_PARM_FORMAT);

    connectKey = "127.0.0.1:abc";
    result = Base::ConnectKey2IPPort(connectKey, outIP, &outPort, sizeof(outIP));
    EXPECT_EQ(result, 0);   //atoi returns 0 when an error occurs
}

HWTEST_F(BaseTest, TestSplitCommandToArgsSingleParam, TestSize.Level0) {
    const char* cmdStringLine = "checkserver";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 1);
    EXPECT_EQ(std::string(argv[0]), "checkserver");
}

HWTEST_F(BaseTest, TestSplitCommandToArgsMultParam, TestSize.Level0) {
    const char* cmdStringLine = "target mount";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 2);
    EXPECT_EQ(std::string(argv[0]), "target");
    EXPECT_EQ(std::string(argv[1]), "mount");
}

HWTEST_F(BaseTest, TestSplitCommandToArgsQuotoArgs, TestSize.Level0) {
    const char* cmdStringLine = "\"shell ls\"";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 1);
    EXPECT_EQ(std::string(argv[0]), "shell ls");
}

HWTEST_F(BaseTest, TestSplitCommandToArgsMultQuoteArgs, TestSize.Level0) {
    const char* cmdStringLine = "\"shell\" \"ls\"";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 2);
    EXPECT_EQ(std::string(argv[0]), "shell");
    EXPECT_EQ(std::string(argv[1]), "ls");
}

HWTEST_F(BaseTest, TestSplitCommandToArgsUnmatchQuotesArgs, TestSize.Level0) {
    const char* cmdStringLine = "\"shell ls";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 1);
    EXPECT_EQ(std::string(argv[0]), "shell ls");
}

HWTEST_F(BaseTest, TestSplitCommandToArgsNullArgs, TestSize.Level0) {
    const char* cmdStringLine = "";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_EQ(argv, nullptr);
    EXPECT_EQ(slotIndex, 0);
}

HWTEST_F(BaseTest, TestSplitCommandToQuoteNullArgs, TestSize.Level0) {
    const char* cmdStringLine = "\"\"";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 1);
    EXPECT_EQ(std::string(argv[0]), "");
}

HWTEST_F(BaseTest, TestSplitCommandToArgsTabArgs, TestSize.Level0) {
    const char* cmdStringLine = "shell\tls";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 2);
    EXPECT_EQ(std::string(argv[0]), "shell");
    EXPECT_EQ(std::string(argv[1]), "ls");
}

HWTEST_F(BaseTest, TestSplitCommandToArgsSpacesArgs, TestSize.Level0) {
    const char* cmdStringLine = "      \t\n\rshell ls";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 2);
    EXPECT_EQ(std::string(argv[0]), "shell");
    EXPECT_EQ(std::string(argv[1]), "ls");
}

HWTEST_F(BaseTest, TestSplitCommandToArgsLongArgs, TestSize.Level0) {
    const char* cmdStringLine = "file send -b com.myapp ./aaaa ./bbbb";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 6);
    EXPECT_EQ(std::string(argv[0]), "file");
    EXPECT_EQ(std::string(argv[1]), "send");
    EXPECT_EQ(std::string(argv[2]), "-b");
    EXPECT_EQ(std::string(argv[3]), "com.myapp");
    EXPECT_EQ(std::string(argv[4]), "./aaaa");
    EXPECT_EQ(std::string(argv[5]), "./bbbb");
}

HWTEST_F(BaseTest, TestRunPipeComandBase, TestSize.Level0) {
    const char* cmdString = "echo \"hello world\"";
    char outBuf[BUF_SIZE_MEDIUM] = "";
    bool result = Base::RunPipeComand(cmdString, outBuf, sizeof(outBuf), true);

    EXPECT_TRUE(result);
    EXPECT_EQ(std::string(outBuf), "hello world");
}

HWTEST_F(BaseTest, TestRunPipeComandNullArgs, TestSize.Level0) {
    const char* cmdString = "echo \"hello world\"";
    char *outBuf = nullptr;
    bool result = Base::RunPipeComand(cmdString, outBuf, sizeof(outBuf), true);

    EXPECT_FALSE(result);
}

HWTEST_F(BaseTest, TestRunPipeComandSizeOutBufZero, TestSize.Level0) {
    const char* cmdString = "echo \"hello world\"";
    char *outBuf = nullptr;
    bool result = Base::RunPipeComand(cmdString, outBuf, 0, true);

    EXPECT_FALSE(result);
}

HWTEST_F(BaseTest, TestSplitString, TestSize.Level0) {
    const string origString = "a,b,c";
    const string seq = ",";
    vector<string> resultStrings;
    Base::SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 3);
    EXPECT_EQ(resultStrings[0], "a");
    EXPECT_EQ(resultStrings[1], "b");
    EXPECT_EQ(resultStrings[2], "c");
}

HWTEST_F(BaseTest, TestSplitStringNullArgs, TestSize.Level0) {
    const string origString = "";
    const string seq = ",";
    vector<string> resultStrings;
    Base::SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 0);
}

HWTEST_F(BaseTest, TestSplitStringLeadingSep, TestSize.Level0) {
    const string origString = ",b,c";
    const string seq = ",";
    vector<string> resultStrings;
    Base::SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 2);
    EXPECT_EQ(resultStrings[0], "b");
    EXPECT_EQ(resultStrings[1], "c");
}

HWTEST_F(BaseTest, TestSplitStringTrailingSep, TestSize.Level0) {
    const string origString = "a,,,,,b,,,,,c";
    const string seq = ",";
    vector<string> resultStrings;
    Base::SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 3);
    EXPECT_EQ(resultStrings[0], "a");
    EXPECT_EQ(resultStrings[1], "b");
    EXPECT_EQ(resultStrings[2], "c");
}

HWTEST_F(BaseTest, TestSplitStringNoSeq, TestSize.Level0) {
    const string origString = "abcdefg";
    const string seq = ",";
    vector<string> resultStrings;
    Base::SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 1);
    EXPECT_EQ(resultStrings[0], "abcdefg");
}

HWTEST_F(BaseTest, TestGetShellPath, TestSize.Level0) {
    string expect = "/bin/sh";
    string result = Base::GetShellPath();

    EXPECT_EQ(result, expect);
}

HWTEST_F(BaseTest, TestStringEndsWith, TestSize.Level0) {
    string s = "hello world";
    string sub = "world";
    EXPECT_EQ(Base::StringEndsWith(s, sub), 1);

    sub = "test";
    EXPECT_EQ(Base::StringEndsWith(s, sub), 0);

    sub = "";
    EXPECT_EQ(Base::StringEndsWith(s, sub), 1);

    sub = "hello world!!!";
    EXPECT_EQ(Base::StringEndsWith(s, sub), 0);

    s = "abcdefg";
    sub = "bcd";
    EXPECT_EQ(Base::StringEndsWith(s, sub), 0);
}

HWTEST_F(BaseTest, TestGetPathWithoutFilename, TestSize.Level0) {
    string s = "/datalocal/tmp/test.txt";
    string expect = "/datalocal/tmp/";
    string result = Base::GetPathWithoutFilename(s);

    EXPECT_EQ(result, expect);
}

HWTEST_F(BaseTest, TestGetPathWithoutFilenameEndWithSep, TestSize.Level0) {
    string s = "/datalocal/tmp/";
    string result = Base::GetPathWithoutFilename(s);

    EXPECT_EQ(result, s);
}

HWTEST_F(BaseTest, TestGetPathWithoutFilenameEmpty, TestSize.Level0) {
    string s = "";
    string result = Base::GetPathWithoutFilename(s);

    EXPECT_EQ(result, s);
}

HWTEST_F(BaseTest, TestGetPathWithoutFilenameMultSep, TestSize.Level0) {
    string s = "/datalocal//tmp/test.txt";
    string expect = "/datalocal//tmp/";
    string result = Base::GetPathWithoutFilename(s);

    EXPECT_EQ(result, expect);
}

HWTEST_F(BaseTest, TestBuildErrorString, TestSize.Level0) {
    const char* op = "Open";
    const char* localPath = "/system/bin/ls";
    const char* err = "Permisson dined";
    string str;
    Base::BuildErrorString(localPath, op, err, str);

    EXPECT_EQ(str, "Open /system/bin/ls failed, Permisson dined");

    Base::BuildErrorString("", "", "", str);
    EXPECT_EQ(str, "  failed, ");
}

HWTEST_F(BaseTest, TestBase64EncodeBufNormal, TestSize.Level0) {
    const uint8_t input[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    const int length = sizeof(input) / sizeof(input[0]);
    uint8_t bufOut[BUF_SIZE_DEFAULT];

    int outputLength = Base::Base64EncodeBuf(input, length, bufOut);
    EXPECT_EQ(outputLength, 8);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(bufOut), outputLength), "SGVsbG8=");
}

HWTEST_F(BaseTest, TestBase64EncodeBufEmpty, TestSize.Level0) {
    const uint8_t input[] = {};
    const int length = 0;
    uint8_t bufOut[BUF_SIZE_DEFAULT];

    int outputLength = Base::Base64EncodeBuf(input, length, bufOut);
    EXPECT_EQ(outputLength, 0);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(bufOut), outputLength), "");
}

HWTEST_F(BaseTest, TestBase64EncodeNormal, TestSize.Level0) {
    const uint8_t input[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    const int length = sizeof(input) / sizeof(input[0]);
    vector<uint8_t> encode = { 'S', 'G', 'V', 's', 'b', 'G', '8', '=' };
    vector<uint8_t> result = Base::Base64Encode(input, length);

    EXPECT_EQ(result, encode);
}

HWTEST_F(BaseTest, TestBase64EncodeEmpty, TestSize.Level0) {
    const uint8_t input[] = {};
    const int length = 0;
    vector<uint8_t> result = Base::Base64Encode(input, length);

    EXPECT_EQ(result.size(), 0);

    result = Base::Base64Encode(nullptr, length);
    EXPECT_EQ(result.size(), 0);
}

HWTEST_F(BaseTest, TestBase64DecodeBufPadding, TestSize.Level0) {
    const uint8_t input[] = "SGVsbG8=";
    int length = strlen(reinterpret_cast<char *>(const_cast<uint8_t *>(input)));
    uint8_t bufOut[BUF_SIZE_DEFAULT] = {};

    int outputLength = Base::Base64DecodeBuf(input, length, bufOut);
    EXPECT_EQ(outputLength, 5); // 5 : "Hello"
    EXPECT_EQ(std::string(reinterpret_cast<char*>(bufOut), outputLength), "Hello");

    const uint8_t input2[] = "SGVsbG8==";
    int length2 = strlen(reinterpret_cast<char *>(const_cast<uint8_t *>(input)));
    uint8_t bufOut2[BUF_SIZE_DEFAULT] = {};

    int outputLength2 = Base::Base64DecodeBuf(input2, length2, bufOut2);
    EXPECT_EQ(outputLength2, 4);  // 5 : "Hell"
    EXPECT_EQ(std::string(reinterpret_cast<char*>(bufOut), outputLength2), "Hell");
}

HWTEST_F(BaseTest, TestBase64DecodeBufSpecial, TestSize.Level0) {
    const uint8_t input[] = "Q2F0Y2xlIHRlc3Q=";    // "Catecle test"
    int length = strlen(reinterpret_cast<char *>(const_cast<uint8_t *>(input)));
    uint8_t bufOut[BUF_SIZE_DEFAULT];

    int outputLength = Base::Base64DecodeBuf(input, length, bufOut);
    EXPECT_EQ(outputLength, 11);    // 11 : "Catecle test"
    EXPECT_EQ(std::string(reinterpret_cast<char*>(bufOut), outputLength), "Catcle test");
}

HWTEST_F(BaseTest, TestBase64DecodeBufEmpty, TestSize.Level0) {
    const uint8_t* input = nullptr;
    int length = 0;
    uint8_t bufOut[BUF_SIZE_DEFAULT];

    int outputLength = Base::Base64DecodeBuf(input, length, bufOut);
    EXPECT_EQ(outputLength, 0);
}

HWTEST_F(BaseTest, TestBase64Decode, TestSize.Level0) {
    const uint8_t input[] = "SGVsbG8=";
    int length = strlen(reinterpret_cast<char *>(const_cast<uint8_t *>(input)));
    string result = Base::Base64Decode(input, length);
    EXPECT_EQ(result, "Hello");
}

HWTEST_F(BaseTest, TestBase64DecodeEmpty, TestSize.Level0) {
    const uint8_t* input = nullptr;
    int length = 0;
    string result = Base::Base64Decode(input, length);
    EXPECT_EQ(result, "");
}

HWTEST_F(BaseTest, TestBase64DecodeInvalidArgs, TestSize.Level0) {
    const uint8_t input[] = "XYZ%";
    int length = strlen(reinterpret_cast<char *>(const_cast<uint8_t *>(input)));
    string result = Base::Base64Decode(input, length);
    EXPECT_EQ(result, "");
}

HWTEST_F(BaseTest, TestReverseBytes, TestSize.Level0) {
    uint8_t start[] = { 0x12, 0x34, 0x56, 0x78};
    int size = sizeof(start) / sizeof(start[0]);
    Base::ReverseBytes(start, size);

    uint8_t result[] = { 0x78, 0x56, 0x34, 0x12};
    EXPECT_EQ(start[0], result[0]);
    EXPECT_EQ(start[1], result[1]);
    EXPECT_EQ(start[2], result[2]);
    EXPECT_EQ(start[3], result[3]);
}

HWTEST_F(BaseTest, TestReverseBytesZero, TestSize.Level0) {
    uint8_t start[] = { 0x00 };
    int size = 0;
    Base::ReverseBytes(start, size);

    EXPECT_EQ(start[0], 0x00);
}

HWTEST_F(BaseTest, TestConvert2HexStr, TestSize.Level0) {
    uint8_t arr[] = { 0x01, 0x02, 0x03 };
    int length = sizeof(arr) / sizeof(arr[0]);
    string result = Base::Convert2HexStr(arr, length);

    EXPECT_EQ(result, "01:02:03");
}

HWTEST_F(BaseTest, TestConvert2HexStrSingleByte, TestSize.Level0) {
    uint8_t arr[] = { 0xFF };
    int length = sizeof(arr) / sizeof(arr[0]);
    string result = Base::Convert2HexStr(arr, length);

    EXPECT_EQ(result, "FF");
}

HWTEST_F(BaseTest, TestConvert2HexStrZeroLength, TestSize.Level0) {
    uint8_t arr[] = {};
    int length = 0;
    string result = Base::Convert2HexStr(arr, length);

    EXPECT_EQ(result, "");
}

HWTEST_F(BaseTest, TestConvert2HexStrUp, TestSize.Level0) {
    uint8_t arr[] = { 0xab, 0xcd, 0xef };
    int length = sizeof(arr) / sizeof(arr[0]);
    string result = Base::Convert2HexStr(arr, length);

    EXPECT_EQ(result, "AB:CD:EF");
}

HWTEST_F(BaseTest, TestConvert2HexStrLargeArray, TestSize.Level0) {
    const int size = 1024;
    uint8_t *data = new uint8_t[size];

    for (int i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(i);
    }

    string result = Base::Convert2HexStr(data, size);

    EXPECT_NE(result.find("00:01:02"), std::string::npos);
    EXPECT_NE(result.find("FE:FF"), std::string::npos);

    delete[] data;
}

HWTEST_F(BaseTest, TestBasicStringFormat, TestSize.Level0) {
    {
        const char* formater = "The Number is %d";
        int number = 123;
        string result = Base::StringFormat(formater, number);
        EXPECT_EQ(result, "The Number is 123");
    }

    {
        const char* formater = "The Number is %.2f";
        double number = 3.14159;
        string result = Base::StringFormat(formater, number);
        EXPECT_EQ(result, "The Number is 3.14");
    }

    {
        const char* formater = "Hello %s";
        const char* name = "World";
        string result = Base::StringFormat(formater, name);
        EXPECT_EQ(result, "Hello World");
    }
}

HWTEST_F(BaseTest, TestChineseStringFormat, TestSize.Level0) {
    const char* formater = "中文：%s";
    const char* name = "你好，世界！";
    string result = Base::StringFormat(formater, name);
    EXPECT_EQ(result, "中文：你好，世界！");
}

HWTEST_F(BaseTest, TestStringToFeatureSetCase1, TestSize.Level0) {
    const Base::HdcFeatureSet features = {"feature1", "feature2"};
    string feature = "feature1";
    EXPECT_TRUE(Base::IsSupportFeature(features, feature));
}

HWTEST_F(BaseTest, TestStringToFeatureSetCase2, TestSize.Level0) {
    const Base::HdcFeatureSet features = {"feature1", "feature2"};
    string feature = "feature3";
    EXPECT_FALSE(Base::IsSupportFeature(features, feature));
}

HWTEST_F(BaseTest, TestStringToFeatureSetCase3, TestSize.Level0) {
    const Base::HdcFeatureSet features = {};
    string feature = "feature3";
    EXPECT_FALSE(Base::IsSupportFeature(features, feature));
}

HWTEST_F(BaseTest, TestUpdateHeartbeatSwitchCacheNoSetEnv, TestSize.Level0) {
    Base::g_heartbeatSwitch = false;
    unsetenv(ENV_SERVER_HEARTBEAT.c_str());
    Base::UpdateHeartbeatSwitchCache();
    EXPECT_TRUE(Base::g_heartbeatSwitch);
}

HWTEST_F(BaseTest, TestUpdateHeartbeatSwitchCacheEmptyEnv, TestSize.Level0) {
    Base::g_heartbeatSwitch = false;
    setenv(ENV_SERVER_HEARTBEAT.c_str(), "", 1);
    Base::UpdateHeartbeatSwitchCache();
    EXPECT_TRUE(Base::g_heartbeatSwitch);
    unsetenv(ENV_SERVER_HEARTBEAT.c_str());
}

HWTEST_F(BaseTest, TestUpdateHeartbeatSwitchCache, TestSize.Level0) {
    Base::g_heartbeatSwitch = false;
    setenv(ENV_SERVER_HEARTBEAT.c_str(), "1", 1);
    Base::UpdateHeartbeatSwitchCache();
    EXPECT_FALSE(Base::g_heartbeatSwitch);
    unsetenv(ENV_SERVER_HEARTBEAT.c_str());
}

HWTEST_F(BaseTest, TestGetheartbeatSwitch, TestSize.Level0) {
    Base::g_heartbeatSwitch = false;
    EXPECT_FALSE(Base::GetheartbeatSwitch());

    Base::g_heartbeatSwitch = true;
    EXPECT_TRUE(Base::GetheartbeatSwitch());
}

HWTEST_F(BaseTest, TestUpdateEncrpytTCPCacheNoSetEnv, TestSize.Level0) {
    Base::g_encryptTCPSwitch = false;
    unsetenv(ENV_ENCRYPT_CHANNEL.c_str());
    Base::UpdateEncrpytTCPCache();
    EXPECT_FALSE(Base::g_encryptTCPSwitch);
}

HWTEST_F(BaseTest, TestUpdateEncrpytTCPCacheEmptyEnv, TestSize.Level0) {
    Base::g_encryptTCPSwitch = false;
    setenv(ENV_ENCRYPT_CHANNEL.c_str(), "0", 1);
    Base::UpdateEncrpytTCPCache();
    EXPECT_FALSE(Base::g_encryptTCPSwitch);
    unsetenv(ENV_ENCRYPT_CHANNEL.c_str());
}

HWTEST_F(BaseTest, TestUpdateEncrpytTCPCache, TestSize.Level0) {
    Base::g_encryptTCPSwitch = false;
    setenv(ENV_ENCRYPT_CHANNEL.c_str(), "1", 1);
    Base::UpdateEncrpytTCPCache();
    EXPECT_TRUE(Base::g_encryptTCPSwitch);
    unsetenv(ENV_ENCRYPT_CHANNEL.c_str());
}

HWTEST_F(BaseTest, TestGetEncrpytTCPSwitch, TestSize.Level0) {
    Base::g_encryptTCPSwitch = false;
    EXPECT_FALSE(Base::GetEncrpytTCPSwitch());
    
    Base::g_encryptTCPSwitch = true;
    EXPECT_TRUE(Base::GetEncrpytTCPSwitch());
}

HWTEST_F(BaseTest, TestUpdateCmdLogSwitchNoSetEnv, TestSize.Level0) {
    Base::g_cmdlogSwitch = false;
    unsetenv(ENV_SERVER_CMD_LOG.c_str());
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::g_cmdlogSwitch);
}

HWTEST_F(BaseTest, TestUpdateCmdLogSwitchEmptyEnv, TestSize.Level0) {
    Base::g_cmdlogSwitch = false;
    setenv(ENV_SERVER_CMD_LOG.c_str(), "0", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::g_cmdlogSwitch);
    unsetenv(ENV_SERVER_CMD_LOG.c_str());
}

HWTEST_F(BaseTest, TestUpdateCmdLogSwitchInvalidEnv, TestSize.Level0) {
    Base::g_cmdlogSwitch = false;
    setenv(ENV_SERVER_CMD_LOG.c_str(), "1234", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::g_cmdlogSwitch);
    unsetenv(ENV_SERVER_CMD_LOG.c_str());
}

HWTEST_F(BaseTest, TestUpdateCmdLogSwitch, TestSize.Level0) {
    Base::g_cmdlogSwitch = false;
    setenv(ENV_SERVER_CMD_LOG.c_str(), "1", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_TRUE(Base::g_cmdlogSwitch);
    unsetenv(ENV_SERVER_CMD_LOG.c_str());
}

HWTEST_F(BaseTest, TestGetCmdLogSwitch, TestSize.Level0) {
    Base::g_cmdlogSwitch = false;
    EXPECT_FALSE(Base::GetCmdLogSwitch());
    
    Base::g_cmdlogSwitch = true;
    EXPECT_TRUE(Base::GetCmdLogSwitch());
}

HWTEST_F(BaseTest, TestWriteToFileSuccessfully, TestSize.Level0) {
    const string fileName = "/data/local/tmp/1.txt";
    const string content = "Hello, World!";
    const string expectedContent = content;

    remove(fileName.c_str());
    EXPECT_TRUE(Base::WriteToFile(fileName, content, std::ios_base::out));

    std::ifstream file(fileName);
    string actualContext;
    getline(file, actualContext);

    EXPECT_EQ(expectedContent, actualContext);
    remove(fileName.c_str());
}

HWTEST_F(BaseTest, TestWriteToFileSuccessfullyCase2, TestSize.Level0) {
    const string fileName = "/data/local/tmp/2.txt";
    const string content = "First line\nSecend line";
    const string expectedContent = content;

    remove(fileName.c_str());
    EXPECT_TRUE(Base::WriteToFile(fileName, content, std::ios_base::app));

    std::ifstream file(fileName);
    string actualContext;
    string line;
    while (getline(file, line)) {
        actualContext += line + "\n";
    }

    EXPECT_EQ(expectedContent + "\n", actualContext);
    remove(fileName.c_str());
}

HWTEST_F(BaseTest, TestWriteToFileSuccessfullyFileExist, TestSize.Level0) {
    const string fileName = "/data/local/tmp/3.txt";
    const string FirstLine = "First line\n";
    const string SecendLine = "Secend line";
    const string expectedContent = FirstLine + SecendLine;

    remove(fileName.c_str());
    {
        std::ofstream file(fileName);
        file << FirstLine;
    }

    EXPECT_TRUE(Base::WriteToFile(fileName, SecendLine, std::ios_base::app));

    std::ifstream file(fileName);
    string actualContext;
    string line;
    while (getline(file, line)) {
        actualContext += line + "\n";
    }

    EXPECT_EQ(expectedContent + "\n", actualContext);
    remove(fileName.c_str());
}

HWTEST_F(BaseTest, TestWriteToFileSuccessfullyPathInvalid, TestSize.Level0) {
    const string fileName = "/invalidpath/5.txt";
    const string content = "Hello, World!";

    EXPECT_FALSE(Base::WriteToFile(fileName, content, std::ios_base::app));
}

#ifndef _WIN32
void CreateTestFile(const string& filePath)
{
    std::ofstream file(filePath);
    if (file.is_open()) {
        file  << "Hello World!";
        file.close();
    }
}

void CleanupTestFiles(const string& filePath, const string& fileName)
{
    (void)remove((filePath + fileName).c_str());
    (void)remove((filePath + "test.tar.gz").c_str());
    (void)rmdir(filePath.c_str());
}

HWTEST_F(BaseTest, TestCompressLogFileSourceFileNotExist, TestSize.Level0) {
    const string filePath = "/data/local/tmp/testDir/";
    const string sourceFileName = "nonexistent.txt";
    const string targetFileName = "test.tar.gz";

    mkdir(filePath.c_str(), 0777);
    EXPECT_FALSE(Base::CompressLogFile(filePath, sourceFileName, targetFileName));

    CleanupTestFiles(filePath, sourceFileName);
}

HWTEST_F(BaseTest, TestCompressLogFileTargetEmpty, TestSize.Level0) {
    const string filePath = "/data/local/tmp/testDir/";
    const string sourceFileName = "test.txt";
    const string targetFileName = "";

    mkdir(filePath.c_str(), 0777);
    CreateTestFile(filePath + sourceFileName);
    EXPECT_FALSE(Base::CompressLogFile(filePath, sourceFileName, targetFileName));

    CleanupTestFiles(filePath, sourceFileName);
}

HWTEST_F(BaseTest, TestCompressLogFile, TestSize.Level0) {
    const string filePath = "/data/local/tmp/testDir/";
    const string sourceFileName = "test.txt";
    const string targetFileName = "test.tar.gz";

    mkdir(filePath.c_str(), 0777);
    CreateTestFile(filePath + sourceFileName);

    EXPECT_TRUE(Base::CompressLogFile(filePath, sourceFileName, targetFileName));
    EXPECT_TRUE(access((filePath + targetFileName).c_str(), F_OK) == 0);

    CleanupTestFiles(filePath, sourceFileName);
}

HWTEST_F(BaseTest, TestCompressLogFileInvalidPath, TestSize.Level0) {
    const string filePath = "/invalid/";
    const string sourceFileName = "/data/local/tmp/test.txt";
    const string targetFileName = "test.tar.gz";

    CreateTestFile(sourceFileName);

    EXPECT_FALSE(Base::CompressLogFile(filePath, sourceFileName, targetFileName));

    CleanupTestFiles(filePath, sourceFileName);
}

HWTEST_F(BaseTest, TestCompressCmdLogAndRemoveSourceFileNotExist, TestSize.Level0) {
    const string filePath = "/data/local/tmp/testDir/";
    const string sourceFileName = "nonexistent.txt";
    const string targetFileName = "test.tar.gz";

    mkdir(filePath.c_str(), 0777);
    EXPECT_FALSE(Base::CompressCmdLogAndRemove(filePath, sourceFileName, targetFileName));

    CleanupTestFiles(filePath, sourceFileName);
}

HWTEST_F(BaseTest, TestCompressCmdLogAndRemove, TestSize.Level0) {
    const string filePath = "/data/local/tmp/testDir/";
    const string sourceFileName = "test.txt";
    const string targetFileName = "test.tar.gz";

    mkdir(filePath.c_str(), 0777);
    CreateTestFile(filePath + sourceFileName);

    EXPECT_TRUE(Base::CompressCmdLogAndRemove(filePath, sourceFileName, targetFileName));
    EXPECT_TRUE(access((filePath + targetFileName).c_str(), F_OK) == 0);
    EXPECT_FALSE(access((filePath + sourceFileName).c_str(), F_OK) == 0);

    CleanupTestFiles(filePath, sourceFileName);
}
#endif

void CreateTestFileAndSetFileSize(const string& fileName, size_t size)
{
    std::ofstream file(fileName, std::ios::binary | std::ios::out);
    if (file.is_open()) {
        file.seekp(size - 1);
        file.put('\0');
        file.close();
    }
}

HWTEST_F(BaseTest, TestIsFileSizeLessThan, TestSize.Level0) {
    const string fileName = "/data/local/tmp/test.txt";
    const size_t fileMaxSize = 1024;

    {
        CreateTestFileAndSetFileSize(fileName, 512);
        EXPECT_TRUE(Base::IsFileSizeLessThan(fileName, fileMaxSize));
        (void)remove(fileName.c_str());
    }

    {
        CreateTestFileAndSetFileSize(fileName, 2048);
        EXPECT_FALSE(Base::IsFileSizeLessThan(fileName, fileMaxSize));
        (void)remove(fileName.c_str());
    }
}

HWTEST_F(BaseTest, TestSetIsServerFlag, TestSize.Level0) {
    Base::g_isServer = false;
    Base::SetIsServerFlag(true);
    EXPECT_TRUE(Base::g_isServer);
    
    Base::SetIsServerFlag(false);
    EXPECT_FALSE(Base::g_isServer);
}

} // namespace Hdc