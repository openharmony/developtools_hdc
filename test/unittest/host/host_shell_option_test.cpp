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
#include "client.h"

using namespace testing;
using namespace testing::ext;

namespace Hdc {

class HostShellOptionTest : public Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
public:
    HdcClient *hdcClient;
};

void HostShellOptionTest::SetUpTestCase()
{
#ifdef UT_DEBUG
    Base::SetLogLevel(LOG_ALL);
#else
    Base::SetLogLevel(LOG_OFF);
#endif
}
void HostShellOptionTest::TearDownTestCase() {}
void HostShellOptionTest::SetUp() {}
void HostShellOptionTest::TearDown() {}


HWTEST_F(HostShellOptionTest, TestFormatParametersToTlv_1, TestSize.Level0)
{
    struct TranslateCommand::FormatCommand formatCommand = {};
    string input = "";
    string stringError;
    bool result = HostShellOption::FormatParametersToTlv(
        input, CMDSTR_SHELL_EX.size() - 1, formatCommand.parameters, stringError);

    EXPECT_EQ(result, false);
    EXPECT_EQ(stringError, "[E003007] Internal error: Invalid option parameters");
}

HWTEST_F(HostShellOptionTest, TestFormatParametersToTlv_2, TestSize.Level0)
{
    struct TranslateCommand::FormatCommand formatCommand = {};
    string input = "shell ls -l";
    string stringError;
    bool result = HostShellOption::FormatParametersToTlv(
        input, CMDSTR_SHELL_EX.size() - 1, formatCommand.parameters, stringError);

    EXPECT_EQ(result, true);
}

HWTEST_F(HostShellOptionTest, TestParameterToTlv_1, TestSize.Level0)
{
    TlvBuf tb(Base::REGISTERD_TAG_SET);
    struct TranslateCommand::FormatCommand formatCommand = {};
    string parameters = "shell -b com.myapptest ls -l";
    string stringError;
    int argc = 0;
    char **argv = Base::SplitCommandToArgs(parameters.c_str() + (CMDSTR_SHELL_EX.size() - 1), &argc);
    ASSERT_TRUE(argv != nullptr);
    ASSERT_TRUE(argc > 0);

    bool result = HostShellOption::ParameterToTlv(argv, argc, tb, stringError);

    EXPECT_EQ(result, true);
}

HWTEST_F(HostShellOptionTest, TestParameterToTlv_2, TestSize.Level0)
{
    TlvBuf tb(Base::REGISTERD_TAG_SET);
    struct TranslateCommand::FormatCommand formatCommand = {};
    string parameters = "shell -b";
    string errMsg = "[E003005] The parameter is missing, correct your input by referring below:\n"
                    "Usage: hdc shell [-b bundlename] [COMMAND...]";
    string stringError;
    int argc = 0;
    char **argv = Base::SplitCommandToArgs(parameters.c_str() + (CMDSTR_SHELL_EX.size() - 1), &argc);
    ASSERT_TRUE(argv != nullptr);
    ASSERT_TRUE(argc > 0);

    bool result = HostShellOption::ParameterToTlv(argv, argc, tb, stringError);

    EXPECT_EQ(result, false);
    EXPECT_EQ(stringError, errMsg);
}

HWTEST_F(HostShellOptionTest, TestParameterToTlv_3, TestSize.Level0)
{
    TlvBuf tb(Base::REGISTERD_TAG_SET);
    struct TranslateCommand::FormatCommand formatCommand = {};
    string parameters = "shell -t ls -l";
    string stringError;
    int argc = 0;
    char **argv = Base::SplitCommandToArgs(parameters.c_str() + (CMDSTR_SHELL_EX.size() - 1), &argc);
    ASSERT_TRUE(argv != nullptr);
    ASSERT_TRUE(argc > 0);

    bool result = HostShellOption::ParameterToTlv(argv, argc, tb, stringError);

    EXPECT_EQ(result, false);
    EXPECT_EQ(stringError, "[E003003] Unsupport shell option: -t");
}

HWTEST_F(HostShellOptionTest, TestConstructShellCommand, TestSize.Level0)
{
    struct TranslateCommand::FormatCommand formatCommand = {};
    string parameters = "shell ls -l";
    int argc = 0;
    char **argv = Base::SplitCommandToArgs(parameters.c_str() + (CMDSTR_SHELL_EX.size() - 1), &argc);
    ASSERT_TRUE(argv != nullptr);
    ASSERT_TRUE(argc > 0);

    string result = HostShellOption::ConstructShellCommand(argv, argc, argc);
    EXPECT_EQ(result, "");

    result = HostShellOption::ConstructShellCommand(argv, 0, argc);
    EXPECT_EQ(result, "ls -l");
}

HWTEST_F(HostShellOptionTest, TestTlvAppendParameter, TestSize.Level0)
{
    TlvBuf tb(Base::REGISTERD_TAG_SET);
    string stringError;
    string shellCommand = "";

    bool result = HostShellOption::TlvAppendParameter(TAG_SHELL_CMD, shellCommand, stringError, tb);
    EXPECT_EQ(result, false);
    EXPECT_EQ(stringError, "[E003002] Unsupport interactive shell command option");

    shellCommand = "error";
    result = HostShellOption::TlvAppendParameter(TAG_SHELL_BUNDLE, shellCommand, stringError, tb);
    EXPECT_EQ(result, false);
    EXPECT_EQ(stringError, "[E003001] Invalid bundle name: error");

    shellCommand = "com.myapptest";
    result = HostShellOption::TlvAppendParameter(TAG_SHELL_BUNDLE, shellCommand, stringError, tb);
    EXPECT_EQ(result, true);
}

}
