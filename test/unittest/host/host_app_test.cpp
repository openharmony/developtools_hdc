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
#include "host_app.h"
#include "host_app_test.h"

using namespace testing::ext;

namespace Hdc {
void HdcHostAppTest::SetUpTestCase() {}
void HdcHostAppTest::TearDownTestCase() {}
void HdcHostAppTest::SetUp()
{
#ifdef UT_DEBUG
    Base::SetLogLevel(LOG_ALL);
#else
    Base::SetLogLevel(LOG_OFF);
#endif
}
void HdcHostAppTest::TearDown() {}

HWTEST_F(HdcHostAppTest, Test_HdcHostApp, TestSize.Level0)
{
    // case1: default struct HTaskInfo with isStableBuf set to true.
    HTaskInfo taskInfo = new TaskInformation();
    taskInfo->isStableBuf = true;
    HdcHostApp hostApp1(taskInfo);
    EXPECT_EQ(hostApp1.commandBegin, CMD_APP_BEGIN);
    EXPECT_EQ(hostApp1.commandData, CMD_APP_DATA);
    EXPECT_EQ(hostApp1.originLocalDir, "");
    EXPECT_TRUE(hostApp1.isStableBuf);

    // case2: default struct HTaskInfo with isStableBuf set to false.
    taskInfo->isStableBuf = false;
    HdcHostApp hostApp2(taskInfo);
    EXPECT_EQ(hostApp2.commandBegin, CMD_APP_BEGIN);
    EXPECT_EQ(hostApp2.commandData, CMD_APP_DATA);
    EXPECT_EQ(hostApp2.originLocalDir, "");
    EXPECT_FALSE(hostApp2.isStableBuf);

    delete taskInfo;
}

HWTEST_F(HdcHostAppTest, Test_CommandDispatch, TestSize.Level0)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcHostApp hostApp(taskInfo);
    uint8_t payload = 10;

    // command invalid.
    EXPECT_FALSE(hostApp.CommandDispatch(CMD_APP_BEGIN, &payload, 10));

    delete taskInfo;
}

HWTEST_F(HdcHostAppTest, Test_BeginInstall, TestSize.Level3)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcHostApp hostApp(taskInfo);
    HdcTransferBase::CtxFile ctxFile;

    // case1: empty input command.
    const char* emptyCommand = "";
    EXPECT_FALSE(hostApp.BeginInstall(&ctxFile, emptyCommand));

    delete taskInfo;
}
}