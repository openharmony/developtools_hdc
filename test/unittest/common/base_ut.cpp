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
class UpdateCmdLogSwitchTest : public ::testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
private:
    char **argv = nullptr;
    int slotIndex = 0;
};

void UpdateCmdLogSwitchTest::SetUpTestCase() {}
void UpdateCmdLogSwitchTest::TearDownTestCase() {}
void UpdateCmdLogSwitchTest::SetUp() {}
void UpdateCmdLogSwitchTest::TearDown() {}
    
HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_12, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "12", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_1, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "1", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_TRUE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_0, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "0", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_a, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "a", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, TestConnectKey2IPv4Port, TestSize.Level0) {
    const char* connectKey = "127.0.0.1:8710";
    char outIP[BUF_SIZE_TINY] = "";
    uint16_t outPort = 0;
    int result = Base::ConnectKey2IPPort(connectKey, outIP, &outPort, sizeof(outIP));

    EXPECT_EQ(result, RET_SUCCESS);
    EXPECT_EQ(std::string(outIP), "127.0.0.1");
    EXPECT_EQ(outPort, 8710);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestConnectKey2IPv6Port, TestSize.Level0) {
    const char* connectKey = "::1:8710";
    char outIP[BUF_SIZE_TINY] = "";
    uint16_t outPort = 0;
    int result = Base::ConnectKey2IPPort(connectKey, outIP, &outPort, sizeof(outIP));

    EXPECT_EQ(result, RET_SUCCESS);
    EXPECT_EQ(std::string(outIP), "::1");
    EXPECT_EQ(outPort, 8710);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestConnectKey2IPInvalid, TestSize.Level0) {
    const char* connectKey = "127.0.0.1";
    char outIP[BUF_SIZE_TINY] = "";
    uint16_t outPort = 0;

    int result = Base::ConnectKey2IPPort(connectKey, outIP, &outPort, sizeof(outIP));
    EXPECT_EQ(result, ERR_PARM_FORMAT);

    connectKey = "127.0.0.1:abc";
    result = Base::ConnectKey2IPPort(connectKey, outIP, &outPort, sizeof(outIP));
    EXPECT_EQ(result, 0);   //atoi returns 0 when an error occurs
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitCommandToArgsSingleParam, TestSize.Level0) {
    const char* cmdStringLine = "checkserver";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 1);
    EXPECT_EQ(std::string(argv[0]), "checkserver");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitCommandToArgsMultParam, TestSize.Level0) {
    const char* cmdStringLine = "target mount";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 2);
    EXPECT_EQ(std::string(argv[0]), "target");
    EXPECT_EQ(std::string(argv[1]), "mount");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitCommandToArgsQuotoArgs, TestSize.Level0) {
    const char* cmdStringLine = "\"shell ls\"";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 1);
    EXPECT_EQ(std::string(argv[0]), "shell ls");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitCommandToArgsMultQuoteArgs, TestSize.Level0) {
    const char* cmdStringLine = "\"shell\" \"ls\"";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 2);
    EXPECT_EQ(std::string(argv[0]), "shell");
    EXPECT_EQ(std::string(argv[1]), "ls");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitCommandToArgsUnmatchQuotesArgs, TestSize.Level0) {
    const char* cmdStringLine = "\"shell ls";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 1);
    EXPECT_EQ(std::string(argv[0]), "shell ls");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitCommandToArgsNullArgs, TestSize.Level0) {
    const char* cmdStringLine = "";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_EQ(argv, nullptr);
    EXPECT_EQ(slotIndex, 0);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitCommandToQuoteNullArgs, TestSize.Level0) {
    const char* cmdStringLine = "\"\"";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 1);
    EXPECT_EQ(std::string(argv[0]), "");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitCommandToArgsTabArgs, TestSize.Level0) {
    const char* cmdStringLine = "shell\tls";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 2);
    EXPECT_EQ(std::string(argv[0]), "shell");
    EXPECT_EQ(std::string(argv[1]), "ls");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitCommandToArgsSpacesArgs, TestSize.Level0) {
    const char* cmdStringLine = "      \t\n\rshell ls";
    argv = Base::SplitCommandToArgs(cmdStringLine, &slotIndex);

    EXPECT_NE(argv, nullptr);
    EXPECT_EQ(slotIndex, 2);
    EXPECT_EQ(std::string(argv[0]), "shell");
    EXPECT_EQ(std::string(argv[1]), "ls");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitCommandToArgsLongArgs, TestSize.Level0) {
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

HWTEST_F(UpdateCmdLogSwitchTest, TestRunPipeComandBase, TestSize.Level0) {
    const char* cmdString = "echo \"hello world\"";
    char outBuf[BUF_SIZE_MEDIUM] = "";
    bool result = Base::RunPipeComand(cmdString, outBuf, sizeof(outBuf), true);

    EXPECT_TRUE(result);
    EXPECT_EQ(std::string(outBuf), "hello world");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestRunPipeComandNullArgs, TestSize.Level0) {
    const char* cmdString = "echo \"hello world\"";
    char *outBuf = nullptr;
    bool result = Base::RunPipeComand(cmdString, outBuf, sizeof(outBuf), true);

    EXPECT_FALSE(result);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestRunPipeComandSizeOutBufZero, TestSize.Level0) {
    const char* cmdString = "echo \"hello world\"";
    char *outBuf = nullptr;
    bool result = Base::RunPipeComand(cmdString, outBuf, 0, true);

    EXPECT_FALSE(result);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitString, TestSize.Level0) {
    const string origString = "a,b,c";
    const string seq = ",";
    vector<string> resultStrings;
    Base::SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 3);
    EXPECT_EQ(resultStrings[0], "a");
    EXPECT_EQ(resultStrings[1], "b");
    EXPECT_EQ(resultStrings[2], "c");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitStringNullArgs, TestSize.Level0) {
    const string origString = "";
    const string seq = ",";
    vector<string> resultStrings;
    Base::SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 0);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitStringLeadingSep, TestSize.Level0) {
    const string origString = ",b,c";
    const string seq = ",";
    vector<string> resultStrings;
    Base::SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 2);
    EXPECT_EQ(resultStrings[0], "b");
    EXPECT_EQ(resultStrings[1], "c");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitStringTrailingSep, TestSize.Level0) {
    const string origString = "a,,,,,b,,,,,c";
    const string seq = ",";
    vector<string> resultStrings;
    Base::SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 3);
    EXPECT_EQ(resultStrings[0], "a");
    EXPECT_EQ(resultStrings[1], "b");
    EXPECT_EQ(resultStrings[2], "c");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestSplitStringNoSeq, TestSize.Level0) {
    const string origString = "abcdefg";
    const string seq = ",";
    vector<string> resultStrings;
    Base::SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 1);
    EXPECT_EQ(resultStrings[0], "abcdefg");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestGetShellPath, TestSize.Level0) {
    string expect = "/bin/sh";
    string result = Base::GetShellPath();

    EXPECT_EQ(result, expect);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestStringEndsWith, TestSize.Level0) {
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

HWTEST_F(UpdateCmdLogSwitchTest, TestGetPathWithoutFilename, TestSize.Level0) {
    string s = "/datalocal/tmp/test.txt";
    string expect = "/datalocal/tmp/";
    string result = Base::GetPathWithoutFilename(s);

    EXPECT_EQ(result, expect);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestGetPathWithoutFilenameEndWithSep, TestSize.Level0) {
    string s = "/datalocal/tmp/";
    string result = Base::GetPathWithoutFilename(s);

    EXPECT_EQ(result, s);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestGetPathWithoutFilenameEmpty, TestSize.Level0) {
    string s = "";
    string result = Base::GetPathWithoutFilename(s);

    EXPECT_EQ(result, s);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestGetPathWithoutFilenameMultSep, TestSize.Level0) {
    string s = "/datalocal//tmp/test.txt";
    string expect = "/datalocal//tmp/";
    string result = Base::GetPathWithoutFilename(s);

    EXPECT_EQ(result, expect);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestBuildErrorString, TestSize.Level0) {
    const char* op = "Open";
    const char* localPath = "/system/bin/ls";
    const char* err = "Permisson dined";
    string str;
    Base::BuildErrorString(localPath, op, err, str);

    EXPECT_EQ(str, "Open /system/bin/ls failed, Permisson dined");

    Base::BuildErrorString("", "", "", str);
    EXPECT_EQ(str, "  failed, ");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestBase64EncodeBufNormal, TestSize.Level0) {
    const uint8_t input[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    const int length = sizeof(input) / sizeof(input[0]);
    uint8_t bufOut[BUF_SIZE_DEFAULT];

    int outputLength = Base::Base64EncodeBuf(input, length, bufOut);
    EXPECT_EQ(outputLength, 8);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(bufOut), outputLength), "SGVsbG8=");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestBase64EncodeBufEmpty, TestSize.Level0) {
    const uint8_t input[] = {};
    const int length = 0;
    uint8_t bufOut[BUF_SIZE_DEFAULT];

    int outputLength = Base::Base64EncodeBuf(input, length, bufOut);
    EXPECT_EQ(outputLength, 0);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(bufOut), outputLength), "");
}

HWTEST_F(UpdateCmdLogSwitchTest, TestBase64EncodeNormal, TestSize.Level0) {
    const uint8_t input[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    const int length = sizeof(input) / sizeof(input[0]);
    vector<uint8_t> encode = { 'S', 'G', 'V', 's', 'b', 'G', '8', '=' };
    vector<uint8_t> result = Base::Base64Encode(input, length);

    EXPECT_EQ(result, encode);
}

HWTEST_F(UpdateCmdLogSwitchTest, TestBase64EncodeEmpty, TestSize.Level0) {
    const uint8_t input[] = {};
    const int length = 0;
    vector<uint8_t> result = Base::Base64Encode(input, length);

    EXPECT_EQ(result.size(), 0);

    result = Base::Base64Encode(nullptr, length);
    EXPECT_EQ(result.size(), 0);
}

} // namespace Hdc