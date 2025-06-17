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
#include "daemon_app.h"
#include "daemon_app_test.h"

using namespace testing::ext;

namespace Hdc {
void HdcDaemonAppTest::SetUpTestCase() {}
void HdcDaemonAppTest::TearDownTestCase() {}
void HdcDaemonAppTest::SetUp()
{
#ifdef UT_DEBUG
    Base::SetLogLevel(LOG_ALL);
#else
    Base::SetLogLevel(LOG_OFF);
#endif
}
void HdcDaemonAppTest::TearDown() {}

HWTEST_F(HdcDaemonAppTest, Test_HdcDaemonApp, TestSize.Level0)
{
    HTaskInfo taskInfo = new TaskInformation();

    // case: default struct HTaskInfo with isStableBuf set to true.
    HdcDaemonApp daemonApp1(taskInfo);
    EXPECT_EQ(daemonApp1.commandBegin, CMD_APP_BEGIN);
    EXPECT_EQ(daemonApp1.commandData, CMD_APP_DATA);
    EXPECT_EQ(daemonApp1.funcAppModFinish, nullptr);

    delete taskInfo;
}

HWTEST_F(HdcDaemonAppTest, Test_CommandDispatch, TestSize.Level0)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcDaemonApp daemonApp(taskInfo);
    uint8_t payload = 10;

    // invalid command.
    EXPECT_FALSE(daemonApp.CommandDispatch(CMD_APP_BEGIN, &payload, 10));

    delete taskInfo;
}

HWTEST_F(HdcDaemonAppTest, Test_ReadyForRelease, TestSize.Level0)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcDaemonApp daemonApp(taskInfo);
    daemonApp.refCount = 1;

    // case1: default valid case.
    EXPECT_FALSE(daemonApp.ReadyForRelease());

    delete taskInfo;
}

HWTEST_F(HdcDaemonAppTest, Test_MakeCtxForAppCheck, TestSize.Level3)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcDaemonApp daemonApp(taskInfo);
    daemonApp.ctxNow.master = true;
    daemonApp.ctxNow.transferConfig.optionalName = "test.hap";

    uint8_t payload = 10;
    daemonApp.MakeCtxForAppCheck(&payload, 10);
    EXPECT_FALSE(daemonApp.ctxNow.master);
    EXPECT_EQ(daemonApp.ctxNow.localPath, "/data/local/tmp/test.hap");

    delete taskInfo;
}
}