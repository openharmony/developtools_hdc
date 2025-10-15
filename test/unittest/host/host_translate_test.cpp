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
#include "host_translate_test.h"
#include "translate.h"

using namespace testing::ext;

namespace Hdc {
void HdcTranslateTest::SetUpTestCase() {}
void HdcTranslateTest::TearDownTestCase() {}
void HdcTranslateTest::SetUp()
{
#ifdef UT_DEBUG
    Base::SetLogLevel(LOG_ALL);
#else
    Base::SetLogLevel(LOG_OFF);
#endif
    formatCommand = new TranslateCommand::FormatCommand();
    formatCommand->cmdFlag = 0;
    formatCommand->parameters = "";
    formatCommand->bJumpDo = false;
}
void HdcTranslateTest::TearDown()
{
    delete formatCommand;
}

HWTEST_F(HdcTranslateTest, TestHdcHelp, TestSize.Level0)
{
    std::string usage = TranslateCommand::Usage();
    EXPECT_FALSE(usage.empty());
    EXPECT_NE(usage.find("-l[0-5]"), string::npos);
    EXPECT_NE(usage.find("checkserver"), string::npos);
    EXPECT_NE(usage.find("tconn key [-remove]"), string::npos);
    EXPECT_NE(usage.find("-s [ip:]port"), string::npos);
    EXPECT_NE(usage.find("-cwd: specify the working directory"), string::npos);
}

HWTEST_F(HdcTranslateTest, TestHdcHelp_OHOS, TestSize.Level0)
{
#ifdef HOST_OHOS
    std::string usage = TranslateCommand::Usage();
    EXPECT_FALSE(usage.empty());
    EXPECT_NE(usage.find("-s [ip:]port | uds"), string::npos);
    EXPECT_EQ(usage.find("keygen FILE"), string::npos);
#endif
}

HWTEST_F(HdcTranslateTest, TestHdcHelpVerbose, TestSize.Level0)
{
    std::string usage = TranslateCommand::Verbose();
    EXPECT_FALSE(usage.empty());
    EXPECT_NE(usage.find("-l[0-5]"), string::npos);
    EXPECT_NE(usage.find("checkserver"), string::npos);
    EXPECT_NE(usage.find("tconn key [-remove]"), string::npos);
    EXPECT_NE(usage.find("-s [ip:]port"), string::npos);
    EXPECT_NE(usage.find("flash commands"), string::npos);
}

HWTEST_F(HdcTranslateTest, TestHdcHelpVerbose_OHOS, TestSize.Level0)
{
#ifdef HOST_OHOS
    std::string usage = TranslateCommand::Verbose();
    EXPECT_FALSE(usage.empty());
    EXPECT_NE(usage.find("-s [ip:]port | uds"), string::npos);
    EXPECT_EQ(usage.find("keygen FILE"), string::npos);
#endif
}

HWTEST_F(HdcTranslateTest, TestTargetConnect, TestSize.Level0)
{
    formatCommand->parameters = "127.0.0.1:12345";
    string result = TranslateCommand::TargetConnect(formatCommand);

    EXPECT_FALSE(formatCommand->bJumpDo);
    EXPECT_EQ(result, "");
    EXPECT_EQ(formatCommand->cmdFlag, CMD_KERNEL_TARGET_CONNECT);
}

HWTEST_F(HdcTranslateTest, TestTargetConnectRemove, TestSize.Level0)
{
    formatCommand->parameters = "127.0.0.1:12345 -remove";
    string result = TranslateCommand::TargetConnect(formatCommand);

    EXPECT_FALSE(formatCommand->bJumpDo);
    EXPECT_EQ(result, "");
    EXPECT_EQ(formatCommand->cmdFlag, CMD_KERNEL_TARGET_DISCONNECT);
    EXPECT_EQ(formatCommand->parameters, "127.0.0.1:12345");
}

HWTEST_F(HdcTranslateTest, TestTargetConnectLongParam, TestSize.Level0)
{
    string longCommand(51, 'a');    // 51 : strlen
    formatCommand->parameters = longCommand;
    string result = TranslateCommand::TargetConnect(formatCommand);

    EXPECT_TRUE(formatCommand->bJumpDo);
    EXPECT_EQ(result, "Error connect key's size");
}

HWTEST_F(HdcTranslateTest, TestTargetConnectInvalidIP, TestSize.Level0)
{
    formatCommand->parameters = "127.0.0.256:12345";
    string result = TranslateCommand::TargetConnect(formatCommand);

    EXPECT_TRUE(formatCommand->bJumpDo);
    EXPECT_EQ(result, "[E001104]:IP address incorrect");
}

HWTEST_F(HdcTranslateTest, TestTargetConnectLocalhost, TestSize.Level0)
{
    formatCommand->parameters = "localhost:12345";
    string result = TranslateCommand::TargetConnect(formatCommand);

    EXPECT_EQ(result, "");
    EXPECT_EQ(formatCommand->parameters, "127.0.0.1:12345");
}

HWTEST_F(HdcTranslateTest, TestTargetConnectPortOutOfRange, TestSize.Level0)
{
    formatCommand->parameters = "127.0.0.1:66666";
    string result = TranslateCommand::TargetConnect(formatCommand);

    EXPECT_TRUE(formatCommand->bJumpDo);
    EXPECT_EQ(result, "IP:Port incorrect");
}

HWTEST_F(HdcTranslateTest, TestForwardPortLs, TestSize.Level0)
{
    const char* inputStr = "fport ls";
    string result = TranslateCommand::ForwardPort(inputStr, formatCommand);

    EXPECT_FALSE(formatCommand->bJumpDo);
    EXPECT_EQ(result, "");
    EXPECT_EQ(formatCommand->cmdFlag, CMD_FORWARD_LIST);
}

HWTEST_F(HdcTranslateTest, TestForwardPortRemove, TestSize.Level0)
{
    const char* inputStr = "fport rm tcp:12345 tcp:54321";
    string result = TranslateCommand::ForwardPort(inputStr, formatCommand);

    EXPECT_EQ(result, "");
    EXPECT_EQ(formatCommand->cmdFlag, CMD_FORWARD_REMOVE);
    EXPECT_FALSE(formatCommand->bJumpDo);
    EXPECT_EQ(formatCommand->parameters, "tcp:12345 tcp:54321");
}

HWTEST_F(HdcTranslateTest, TestForwardPortTcp, TestSize.Level0)
{
    const char* inputStr = "fport tcp:12345 tcp:54321";
    string result = TranslateCommand::ForwardPort(inputStr, formatCommand);

    EXPECT_EQ(result, "");
    EXPECT_EQ(formatCommand->cmdFlag, CMD_FORWARD_INIT);
    EXPECT_FALSE(formatCommand->bJumpDo);
    EXPECT_EQ(formatCommand->parameters, "fport tcp:12345 tcp:54321");
}

HWTEST_F(HdcTranslateTest, TestForwardPortInvalidCommand, TestSize.Level0)
{
    const char* inputStr = "fport invalid";
    string result = TranslateCommand::ForwardPort(inputStr, formatCommand);

    EXPECT_TRUE(formatCommand->bJumpDo);
    EXPECT_EQ(result, "Incorrect forward command");
}

HWTEST_F(HdcTranslateTest, TestForwardPortEmpty, TestSize.Level0)
{
    const char* inputStr = "";
    string result = TranslateCommand::ForwardPort(inputStr, formatCommand);

    EXPECT_TRUE(formatCommand->bJumpDo);
    EXPECT_EQ(result, "Incorrect forward command");
}

HWTEST_F(HdcTranslateTest, TestRunModeNormal, TestSize.Level0)
{
    const char* inputStr = "tmode port 8710";
    string result = TranslateCommand::RunMode(inputStr, formatCommand);

    EXPECT_EQ(result, "");
    EXPECT_EQ(formatCommand->cmdFlag, CMD_UNITY_RUNMODE);
    EXPECT_FALSE(formatCommand->bJumpDo);
    EXPECT_EQ(formatCommand->parameters, "port 8710");
}

HWTEST_F(HdcTranslateTest, TestRunModeClose, TestSize.Level0)
{
    const char* inputStr = "tmode port close";
    string result = TranslateCommand::RunMode(inputStr, formatCommand);

    EXPECT_EQ(result, "");
    EXPECT_EQ(formatCommand->cmdFlag, CMD_UNITY_RUNMODE);
    EXPECT_FALSE(formatCommand->bJumpDo);
    EXPECT_EQ(formatCommand->parameters, "port close");
}

HWTEST_F(HdcTranslateTest, TestRunModeinvalid, TestSize.Level0)
{
    const char* inputStr = "tmode invalid";
    string result = TranslateCommand::RunMode(inputStr, formatCommand);

    EXPECT_EQ(result, "Error tmode command");
    EXPECT_TRUE(formatCommand->bJumpDo);
}

HWTEST_F(HdcTranslateTest, TestRunModePortRangeError, TestSize.Level0)
{
    const char* inputStr = "tmode port 0";
    string result = TranslateCommand::RunMode(inputStr, formatCommand);

    EXPECT_EQ(result, "Incorrect port range");
    EXPECT_TRUE(formatCommand->bJumpDo);
}

HWTEST_F(HdcTranslateTest, TestRunModeMissingPort, TestSize.Level0)
{
    const char* inputStr = "tmode port ";
    string result = TranslateCommand::RunMode(inputStr, formatCommand);

    EXPECT_EQ(result, "Incorrect port range");
    EXPECT_TRUE(formatCommand->bJumpDo);
}

HWTEST_F(HdcTranslateTest, TestRunModeMinPort, TestSize.Level0)
{
    const char* inputStr = "tmode port 1";
    string result = TranslateCommand::RunMode(inputStr, formatCommand);

    EXPECT_EQ(result, "");
    EXPECT_EQ(formatCommand->cmdFlag, CMD_UNITY_RUNMODE);
    EXPECT_FALSE(formatCommand->bJumpDo);
    EXPECT_EQ(formatCommand->parameters, "port 1");
}

HWTEST_F(HdcTranslateTest, TestRunModeMaxPort, TestSize.Level0)
{
    const char* inputStr = "tmode port 65535";
    string result = TranslateCommand::RunMode(inputStr, formatCommand);

    EXPECT_EQ(result, "");
    EXPECT_EQ(formatCommand->cmdFlag, CMD_UNITY_RUNMODE);
    EXPECT_FALSE(formatCommand->bJumpDo);
    EXPECT_EQ(formatCommand->parameters, "port 65535");
}

HWTEST_F(HdcTranslateTest, TestTargetRebootBootloader, TestSize.Level0)
{
    const char* inputStr = "target boot -bootloader";
    TranslateCommand::TargetReboot(inputStr, formatCommand);

    EXPECT_EQ(formatCommand->cmdFlag, CMD_UNITY_REBOOT);
    EXPECT_EQ(formatCommand->parameters, "bootloader");
}

HWTEST_F(HdcTranslateTest, TestTargetReboot, TestSize.Level0)
{
    const char* inputStr = "target boot";
    TranslateCommand::TargetReboot(inputStr, formatCommand);

    EXPECT_EQ(formatCommand->cmdFlag, CMD_UNITY_REBOOT);
    EXPECT_EQ(formatCommand->parameters, "");
}

HWTEST_F(HdcTranslateTest, TestString2FormatCommandHelp, TestSize.Level0)
{
    const char* inputStr = "help";
    string expectResult = TranslateCommand::Usage();
    string result = TranslateCommand::String2FormatCommand(inputStr, strlen(inputStr), formatCommand);

    EXPECT_EQ(formatCommand->cmdFlag, CMD_KERNEL_HELP);
    EXPECT_EQ(result, expectResult += "\n");
}

HWTEST_F(HdcTranslateTest, TestString2FormatCommandServiveKill, TestSize.Level0)
{
#ifdef HOST_OHOS
    const char* inputStr = "kill";
    string result = TranslateCommand::String2FormatCommand(inputStr, strlen(inputStr), formatCommand);

    EXPECT_EQ(formatCommand->cmdFlag, CMD_SERVER_KILL);
#endif
}

HWTEST_F(HdcTranslateTest, TestString2FormatCommandUnkown, TestSize.Level0)
{
    const char* inputStr = "unkown";
    string result = TranslateCommand::String2FormatCommand(inputStr, strlen(inputStr), formatCommand);

    EXPECT_EQ(formatCommand->cmdFlag, 0);
    EXPECT_EQ(result, "Unknown command...\n");
}

HWTEST_F(HdcTranslateTest, TestString2FormatCommandFportLs, TestSize.Level0)
{
    const char* inputStr = "fport ls";
    string result = TranslateCommand::String2FormatCommand(inputStr, strlen(inputStr), formatCommand);

    EXPECT_EQ(formatCommand->cmdFlag, CMD_FORWARD_LIST);
}

HWTEST_F(HdcTranslateTest, TestString2FormatCommandTrackJpid, TestSize.Level0)
{
    const char* inputStr = "track-jpid -p";
    string result = TranslateCommand::String2FormatCommand(inputStr, strlen(inputStr), formatCommand);

    EXPECT_EQ(formatCommand->cmdFlag, CMD_JDWP_TRACK);
    EXPECT_EQ(formatCommand->parameters, "p");
}

}