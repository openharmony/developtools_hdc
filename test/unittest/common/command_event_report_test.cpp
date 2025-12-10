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
#include "command_event_report_test.h"

using namespace testing::ext;

namespace Hdc {
void HdcCommandEventReportTest::SetUpTestCase() {}
void HdcCommandEventReportTest::TearDownTestCase() {}
void HdcCommandEventReportTest::SetUp() {}
void HdcCommandEventReportTest::TearDown() {}
std::string HdcCommandEventReportTest::GetCallerName(Base::Caller caller)
{
    if (caller == Base::Caller::SERVER) {
        return Base::CALLER_SERVER;
    } else if (caller == Base::Caller::CLIENT) {
        return Base::CALLER_CLIENT;
    }
    return Base::CALLER_DAEMON;
}

bool HdcCommandEventReportTest::GetCommandFromInputRaw(const char* inputRaw, std::string &command)
{
    std::vector<std::string> Compare = {
        CMDSTR_SOFTWARE_VERSION, CMDSTR_TARGET_MOUNT, CMDSTR_LIST_JDWP, CMDSTR_SHELL
    };
    std::vector<std::string> nCompare = {
        CMDSTR_SOFTWARE_HELP, CMDSTR_LIST_TARGETS, CMDSTR_SERVICE_KILL,
        CMDSTR_CHECK_SERVER, CMDSTR_CHECK_DEVICE, CMDSTR_HILOG,
        CMDSTR_WAIT_FOR, CMDSTR_CONNECT_TARGET, CMDSTR_SHELL_EX,
        CMDSTR_BUGREPORT, CMDSTR_SHELL + " ", CMDSTR_FILE_SEND,
        CMDSTR_FILE_RECV, CMDSTR_FORWARD_FPORT + " ", CMDSTR_FORWARD_RPORT + " ",
        CMDSTR_APP_INSTALL, CMDSTR_APP_UNINSTALL, CMDSTR_TRACK_JDWP,
        CMDSTR_TARGET_REBOOT, CMDSTR_TARGET_MODE
    };

    for (const auto &elem : Compare) {
        if (!strcmp(inputRaw, elem.c_str())) {
            command = elem;
            return true;
        }
    }
    for (const auto &elem : nCompare) {
        if (!strncmp(inputRaw, elem.c_str(), elem.size())) {
            command = elem;
            return true;
        }
    }

    return false;
}


HWTEST_F(HdcCommandEventReportTest, TestGetCallerName_ServerCall, TestSize.Level0)
{
    EXPECT_EQ(GetCallerName(Base::Caller::SERVER), Base::CALLER_SERVER);
}

HWTEST_F(HdcCommandEventReportTest, TestGetCallerName_ClientCall, TestSize.Level0)
{
    EXPECT_EQ(GetCallerName(Base::Caller::CLIENT), Base::CALLER_CLIENT);
}

HWTEST_F(HdcCommandEventReportTest, TestGetCallerName_DaemonCall, TestSize.Level0)
{
    EXPECT_EQ(GetCallerName(Base::Caller::DAEMON), Base::CALLER_DAEMON);
}

HWTEST_F(HdcCommandEventReportTest, TestGetCommand_Version, TestSize.Level0)
{
    std::string input = "version";
    std::string command;
    EXPECT_TRUE(GetCommandFromInputRaw(input.c_str(), command));
    EXPECT_EQ(command, CMDSTR_SOFTWARE_VERSION);
}

HWTEST_F(HdcCommandEventReportTest, TestGetCommand_BadVersion, TestSize.Level0)
{
    std::string input = "version -a";
    std::string command;
    EXPECT_FALSE(GetCommandFromInputRaw(input.c_str(), command));
}

HWTEST_F(HdcCommandEventReportTest, TestGetCommand_Shell, TestSize.Level0)
{
    std::string input = "shell";
    std::string command;
    EXPECT_TRUE(GetCommandFromInputRaw(input.c_str(), command));
    EXPECT_EQ(command, CMDSTR_SHELL);
}

HWTEST_F(HdcCommandEventReportTest, TestGetCommand_ShellCommand, TestSize.Level0)
{
    std::string input = "shell setenforce";
    std::string command;
    EXPECT_TRUE(GetCommandFromInputRaw(input.c_str(), command));
    EXPECT_EQ(command, CMDSTR_SHELL + " ");
}

HWTEST_F(HdcCommandEventReportTest, TestGetCommand_EmptyInput, TestSize.Level0)
{
    std::string input;
    std::string command;
    EXPECT_FALSE(GetCommandFromInputRaw(input.c_str(), command));
}

HWTEST_F(HdcCommandEventReportTest, TestGetCommand_UnknowCommand, TestSize.Level0)
{
    std::string input = "testUnknowCommand";
    std::string command;
    EXPECT_FALSE(GetCommandFromInputRaw(input.c_str(), command));
}
}