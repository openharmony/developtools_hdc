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
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <unistd.h>
#include <string>

#include "process_handle.h"

using namespace testing::ext;
using namespace Hdc;

class ProcessHandleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(ProcessHandleTest, GetCurrentPid_Success, TestSize.Level0)
{
    uint32_t pid = ProcessHandle::GetCurrentPid();
    EXPECT_GT(pid, 0);
    EXPECT_EQ(pid, getpid());
}

HWTEST_F(ProcessHandleTest, GetExecutablePath_Success, TestSize.Level0)
{
    std::string path = ProcessHandle::GetExecutablePath();
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(path.find("hdc") != std::string::npos);
}

HWTEST_F(ProcessHandleTest, BuildSubserverArgs_Success, TestSize.Level0)
{
    char buf[256];
    setenv("OHOS_HDC_SUBSERVER_LOG_FILE", "10", 1);

    bool result = ProcessHandle::BuildSubserverArgs(buf, sizeof(buf), "0.0.0.0:8080", "serial123", "8080");
    EXPECT_TRUE(result);
    EXPECT_TRUE(strstr(buf, "-s 0.0.0.0:8080") != nullptr);
    EXPECT_TRUE(strstr(buf, "-i serial123") != nullptr);
    EXPECT_TRUE(strstr(buf, "-o 8080") != nullptr);

    unsetenv("OHOS_HDC_SUBSERVER_LOG_FILE");
}

HWTEST_F(ProcessHandleTest, BuildSubserverArgs_NoLogFile, TestSize.Level0)
{
    char buf[256];
    unsetenv("OHOS_HDC_SUBSERVER_LOG_FILE");

    bool result = ProcessHandle::BuildSubserverArgs(buf, sizeof(buf), "127.0.0.1:8123", "test_serial", "8123");
    EXPECT_TRUE(result);
    EXPECT_TRUE(strstr(buf, "-s 127.0.0.1:8123") != nullptr);
    EXPECT_TRUE(strstr(buf, "-i test_serial") != nullptr);
    EXPECT_TRUE(strstr(buf, "-o 8123") != nullptr);
    EXPECT_TRUE(strstr(buf, "-L") == nullptr);
}

HWTEST_F(ProcessHandleTest, BuildSubserverArgs_BufferTooSmall, TestSize.Level1)
{
    char buf[10];

    bool result = ProcessHandle::BuildSubserverArgs(buf, sizeof(buf), "very_long_address", "serial123", "8080");
    EXPECT_FALSE(result);
}

HWTEST_F(ProcessHandleTest, IsValid_EmptyHandle, TestSize.Level0)
{
    ProcessHandle handle;
    EXPECT_FALSE(handle.IsValid());
}

HWTEST_F(ProcessHandleTest, IsAlive_EmptyHandle, TestSize.Level0)
{
    ProcessHandle handle;
    EXPECT_FALSE(handle.IsAlive());
}

HWTEST_F(ProcessHandleTest, GetExitCode_EmptyHandle, TestSize.Level0)
{
    ProcessHandle handle;
    EXPECT_EQ(handle.GetExitCode(), -1);
}