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
#include "host_remove_forward_test.h"

using namespace testing::ext;

namespace Hdc {
void HdcHostRemoveForwardTest::SetUpTestCase() {}
void HdcHostRemoveForwardTest::TearDownTestCase() {}

void HdcHostRemoveForwardTest::SetUp()
{
    std::string addrString = "127.0.0.1:0";
    uv_loop_init(&loopMain);
    server =  new HdcServer(true);
    cls = new HdcServerForClient(true, addrString, nullptr, &loopMain);
    server->clsServerForClient = cls;
    cls->clsServer = server;
}

void HdcHostRemoveForwardTest::TearDown()
{
    delete cls;
    delete server;
    uv_loop_close(&loopMain);
}

void HdcHostRemoveForwardTest::AddForward(uint32_t sessionId)
{
    uint32_t channelId = 1234567890;
    std::string connectKey = "abcdefghijklmn";
    std::string paramStr = "tcp:11111 tcp:22222";
    HdcForwardInformation di;
    HForwardInfo pdiNew = &di;
    pdiNew->sessionId = sessionId;
    pdiNew->channelId = channelId;
    pdiNew->connectKey = connectKey;
    pdiNew->forwardDirection = false;
    pdiNew->taskString = connectKey + "|0|" + paramStr;
    server->AdminForwardMap(OP_ADD, "", pdiNew);
}

HWTEST_F(HdcHostRemoveForwardTest, Test_RemoveFportkey_ForwardKey_Not_Exist, TestSize.Level0)
{
    AddForward(0);
    EXPECT_FALSE(cls->RemoveFportkey("forwardKey not exist"));
}

HWTEST_F(HdcHostRemoveForwardTest, Test_RemoveFportkey_Session_Not_Exist, TestSize.Level0)
{
    AddForward(0);
    EXPECT_TRUE(cls->RemoveFportkey("abcdefghijklmn|0|tcp:11111 tcp:22222"));
}

HWTEST_F(HdcHostRemoveForwardTest, Test_RemoveFportkey_Session_Exist, TestSize.Level0)
{
    HSession hSession = new(std::nothrow) HdcSession();
    hSession->classInstance = this;
    hSession->connType = CONN_USB;
    hSession->classModule = nullptr;
    hSession->isDead = false;
    hSession->sessionId = 1234567890;
    hSession->serverOrDaemon = true;
    hSession->mapTask = new(std::nothrow) map<uint32_t, HTaskInfo>();
    server->AdminSession(OP_ADD, hSession->sessionId, hSession);

    AddForward(hSession->sessionId);
    EXPECT_TRUE(cls->RemoveFportkey("abcdefghijklmn|0|tcp:11111 tcp:22222"));
    delete hSession;
}

HWTEST_F(HdcHostRemoveForwardTest, Test_RemoveForward_No_Rule, TestSize.Level0)
{
    HChannel hChannel = new HdcChannel();
    hChannel->channelId = 1234567890;
    hChannel->connectKey = "abcdefghijklmn";
    EXPECT_FALSE(cls->RemoveForward(hChannel, "tcp:11111 tcp:22223"));
    delete hChannel;
}

HWTEST_F(HdcHostRemoveForwardTest, Test_RemoveForward_Not_Exist, TestSize.Level0)
{
    HChannel hChannel = new HdcChannel();
    hChannel->channelId = 1234567890;
    hChannel->connectKey = "abcdefghijklmn";
    AddForward(0);
    EXPECT_FALSE(cls->RemoveForward(hChannel, "tcp:11111 tcp:22221"));
    delete hChannel;
}

HWTEST_F(HdcHostRemoveForwardTest, Test_RemoveForward_Exist, TestSize.Level0)
{
    HChannel hChannel = new HdcChannel();
    hChannel->channelId = 1234567890;
    hChannel->connectKey = "abcdefghijklmn";
    AddForward(0);
    EXPECT_TRUE(cls->RemoveForward(hChannel, "tcp:11111 tcp:22222"));
    delete hChannel;
}

}
