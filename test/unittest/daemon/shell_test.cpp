/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include "shell.h"
#include "task.h"
#include "base.h"
#include "define_plus.h"
#include "tlv.h"
#include "os_account_manager.h"
#include "shell_test.h"

using namespace testing;
using namespace testing::ext;

namespace {
static const int32_t DEFAULT_USER_ID = 100;
}

namespace Hdc {
void ShellTest::SetUpTestCase()
{
#ifdef UT_DEBUG
    Base::SetLogLevel(LOG_ALL);
#else
    Base::SetLogLevel(LOG_OFF);
#endif
}

void ShellTest::TearDownTestCase() {}

void ShellTest::SetUp()
{
    taskInfo = new TaskInformation();
    taskInfo->channelId = 1;
    taskInfo->sessionId = 1;
    shell = new HdcShell(taskInfo);
}

void ShellTest::TearDown()
{
    if (shell != nullptr) {
        delete shell;
        shell = nullptr;
    }
    if (taskInfo != nullptr) {
        delete taskInfo;
        taskInfo = nullptr;
    }
}

HWTEST_F(ShellTest, TestShellConstruction, TestSize.Level0)
{
    ASSERT_NE(shell, nullptr);
    EXPECT_EQ(shell->taskInfo->channelId, 1);
}

HWTEST_F(ShellTest, TestShellDestruction, TestSize.Level0)
{
    HdcShell *testShell = new HdcShell(taskInfo);
    ASSERT_NE(testShell, nullptr);
    delete testShell;
    testShell = nullptr;
}

HWTEST_F(ShellTest, TestReadyForRelease_Initial, TestSize.Level0)
{
    bool result = shell->ReadyForRelease();
    EXPECT_TRUE(result);
}

HWTEST_F(ShellTest, TestStopTask_Initial, TestSize.Level0)
{
    shell->StopTask();
}

HWTEST_F(ShellTest, TestInitShell_WithValidBundle, TestSize.Level0)
{
    std::string bundleName = "com.example.testapp";
    std::string basePath = DEBUG_BUNDLE_PATH;
    std::vector<int32_t> ids;
    OHOS::ErrCode err = OHOS::AccountSA::OsAccountManager::QueryActiveOsAccountIds(ids);
    int id = -1;
    if (err != 0 || ids.empty()) {
        id = DEFAULT_USER_ID;
    } else {
        id = ids[0];
    }
    basePath += std::to_string(id) + "/debug_hap/";
    
    mkdir(basePath.c_str(), 0755);
    std::string bundlePath = basePath + bundleName;
    mkdir(bundlePath.c_str(), 0755);

    TlvBuf tlvBuf(Base::REGISTERD_TAG_SET);
    tlvBuf.Append(TAG_SHELL_BUNDLE, bundleName);
    uint32_t size = tlvBuf.GetBufSize();
    uint8_t *buf = new (std::nothrow) uint8_t[size];
    tlvBuf.CopyToBuf(buf, size);
    
    bool result = shell->InitShell(buf, size);
    
    EXPECT_TRUE(result);
    
    delete[] buf;
    rmdir(bundlePath.c_str());
    rmdir(basePath.c_str());
}

HWTEST_F(ShellTest, TestInitShell_WithoutBundle, TestSize.Level0)
{
    TlvBuf tlvBuf(Base::REGISTERD_TAG_SET);
    uint32_t size = tlvBuf.GetBufSize();
    uint8_t *buf = new (std::nothrow) uint8_t[size];
    tlvBuf.CopyToBuf(buf, size);
    
    bool result = shell->InitShell(buf, size);
    
    EXPECT_TRUE(result);
    
    delete[] buf;
}

HWTEST_F(ShellTest, TestInitShell_WithEmptyPayload, TestSize.Level0)
{
    bool result = shell->InitShell(nullptr, 0);
    EXPECT_TRUE(result);
}
}
