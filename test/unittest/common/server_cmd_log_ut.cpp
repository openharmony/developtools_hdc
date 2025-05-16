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
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "securec.h"
#include "serial_struct.h"
#include "server_cmd_log.h"

using namespace testing::ext;
using namespace testing;

namespace Hdc {
class ServerCmdLogTest : public ::testing::Test {
    private:
        static void SetUpTestCase(void);
        static void TearDownTestCase(void);
        void SetUp();
        void TearDown();
};
void ServerCmdLogTest::SetUpTestCase() {}
void ServerCmdLogTest::TearDownTestCase() {}
void ServerCmdLogTest::SetUp() {}
void ServerCmdLogTest::TearDown() {}

HWTEST_F(ServerCmdLogTest, PushCmdLogStrTest, TestSize.Level0)
{
    ServerCmdLog log;
    log.PushCmdLogStr("Test Command 1");
    log.PushCmdLogStr("Test Command 2");

    EXPECT_EQ(log.CmdLogStrSize(), 2);
}

HWTEST_F(ServerCmdLogTest, PopCmdLogStrTest, TestSize.Level0)
{
    ServerCmdLog log;
    log.PushCmdLogStr("Test Command 1");
    log.PushCmdLogStr("Test Command 2");

    std::string cmd1 = log.PopCmdLogStr();
    std::string cmd2 = log.PopCmdLogStr();

    EXPECT_EQ(cmd1, "Test Command 1");
    EXPECT_EQ(cmd2, "Test Command 2");
    EXPECT_EQ(log.CmdLogStrSize(), 0);
}

HWTEST_F(ServerCmdLogTest, PopCmdLogStrEmptyQueueTest, TestSize.Level0)
{
    ServerCmdLog log;
    std::string cmd = log.PopCmdLogStr();

    EXPECT_EQ(cmd, "");
}

HWTEST_F(ServerCmdLogTest, LastFlushTimeTest, TestSize.Level0)
{
    ServerCmdLog log;
    log.PushCmdLogStr("Test Command 1");

    auto beforePopTime = log.GetLastFlushTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    log.PopCmdLogStr();
    auto afterPopTime = log.GetLastFlushTime();

    EXPECT_GT(afterPopTime, beforePopTime);
}

HWTEST_F(ServerCmdLogTest, PushCmdLogStrQueueLimitTest, TestSize.Level0)
{
    ServerCmdLog log;
    for (size_t i = 0; i < 1500; ++i) {
        log.PushCmdLogStr("Command " + std::to_string(i));
    }
    EXPECT_GT(log.CmdLogStrSize(), 0);

    log.PushCmdLogStr("Exceeding Command");
    EXPECT_GT(log.CmdLogStrSize(), 0);
}

} // namespace Hdc
