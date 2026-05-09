/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>

#include "subserver_process_info.h"
#include "process_handle.h"
#include <functional>

extern void SetMockIsAlive(std::function<bool(pid_t)> func);
extern void SetMockGetExitCode(std::function<int(pid_t)> func);
extern void ClearMockFunctions();

using namespace testing::ext;
using namespace Hdc;

class SubserverProcessInfoTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        ClearMockFunctions();
    }
};

HWTEST_F(SubserverProcessInfoTest, Constructor_ValidHandle, TestSize.Level0)
{
    auto mockHandle = std::make_unique<ProcessHandle>();
    mockHandle->SetPidForTest(getpid());

    SubserverProcessInfo info(std::move(mockHandle));
    EXPECT_EQ(info.GetSubserverStatus(), SubserverStatus::CONNECTING);
}

HWTEST_F(SubserverProcessInfoTest, Constructor_NullHandle, TestSize.Level0)
{
    SubserverProcessInfo info(nullptr);

    EXPECT_EQ(info.GetSubserverStatus(), SubserverStatus::SUBPROCESS_FAIL);
}

HWTEST_F(SubserverProcessInfoTest, GetSubserverStatus_CurrentProcess, TestSize.Level1)
{
    auto handle = ProcessHandle::SpawnSubprocess("/bin/sleep", "1");
    if (handle) {
        SubserverProcessInfo info(std::move(handle));

        auto status = info.GetSubserverStatus();
        EXPECT_TRUE(status == SubserverStatus::CONNECTING || status == SubserverStatus::SUBSERVER_OTHER_EXIT);

        std::this_thread::sleep_for(std::chrono::seconds(2));

        status = info.GetSubserverStatus();
        EXPECT_EQ(status, SubserverStatus::SUBSERVER_OTHER_EXIT);
    }
}

HWTEST_F(SubserverProcessInfoTest, GetSubserverStatus_InvalidPid, TestSize.Level1)
{
    SetMockIsAlive([](pid_t) { return false; });

    auto mockHandle = std::make_unique<ProcessHandle>();
    mockHandle->SetPidForTest(999999);

    SubserverProcessInfo info(std::move(mockHandle));

    auto status = info.GetSubserverStatus();
    EXPECT_EQ(status, SubserverStatus::SUBSERVER_OTHER_EXIT);

    ClearMockFunctions();
}

HWTEST_F(SubserverProcessInfoTest, GetSubserverStatus_ConnectSuccess, TestSize.Level0)
{
    auto mockHandle = std::make_unique<ProcessHandle>();
    mockHandle->SetPidForTest(getpid());

    SetMockIsAlive([](pid_t) { return true; });

    SubserverProcessInfo info(std::move(mockHandle));

    EXPECT_EQ(info.GetSubserverStatus(), SubserverStatus::CONNECTING);

    std::this_thread::sleep_for(std::chrono::seconds(11));

    EXPECT_EQ(info.GetSubserverStatus(), SubserverStatus::CONNECT_SUCCESS);

    ClearMockFunctions();
}

HWTEST_F(SubserverProcessInfoTest, GetSubserverStatus_ProcessExited, TestSize.Level0)
{
    auto mockHandle = std::make_unique<ProcessHandle>();
    mockHandle->SetPidForTest(getpid());

    SetMockIsAlive([](pid_t) { return false; });
    SetMockGetExitCode([](pid_t) { return 0; });

    SubserverProcessInfo info(std::move(mockHandle));

    EXPECT_EQ(info.GetSubserverStatus(), SubserverStatus::SUBSERVER_OTHER_EXIT);

    ClearMockFunctions();
}

HWTEST_F(SubserverProcessInfoTest, GetSubserverStatus_AbnormalExit, TestSize.Level0)
{
    auto mockHandle = std::make_unique<ProcessHandle>();
    mockHandle->SetPidForTest(getpid());

    SetMockIsAlive([](pid_t) { return false; });
    SetMockGetExitCode([](pid_t) { return -1; });

    SubserverProcessInfo info(std::move(mockHandle));

    auto status = info.GetSubserverStatus();
    EXPECT_TRUE(status == SubserverStatus::SUBSERVER_OTHER_EXIT ||
                status == SubserverStatus::SUBPROCESS_FAIL);

    ClearMockFunctions();
}

