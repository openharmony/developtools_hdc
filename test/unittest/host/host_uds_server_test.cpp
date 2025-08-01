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
#include "host_uds_server_test.h"
#ifdef __OHOS__
#include "server_for_client.h"
#include "uv.h"
#include <securec.h>
#include "define_enum.h"
#include "server.h"
#endif

using namespace testing::ext;

namespace Hdc {
void HdcHostUdsServerTest::SetUpTestCase() {}
void HdcHostUdsServerTest::TearDownTestCase() {}
void HdcHostUdsServerTest::SetUp()
{
}
void HdcHostUdsServerTest::TearDown() {}


HWTEST_F(HdcHostUdsServerTest, Test_AcceptUdsClient, TestSize.Level0)
{
#ifdef __OHOS__
    uv_loop_t loopMain;
    uv_loop_init(&loopMain);
    HdcServerForClient *cls = new HdcServerForClient(true, std::string("uds"), nullptr, &loopMain);
    EXPECT_TRUE(!cls->SetUdsListen());
    sleep(1);
    HdcServerForClient::AcceptUdsClient((uv_stream_t *)&cls->udsListen, 0);
    EXPECT_TRUE(cls->mapChannel.size() == 1);

    delete cls;
#endif
}

HWTEST_F(HdcHostUdsServerTest, Test_AttachChannelInnerForUds, TestSize.Level0)
{
#ifdef __OHOS__
    uv_loop_t loopMain;
    uv_loop_init(&loopMain);

    HdcServer server(true);
    HdcServerForClient *clsServerForClient = new HdcServerForClient(true, "uds", &server, &loopMain);
    server.clsServerForClient = clsServerForClient;
    HSession hSession = server.MallocSession(true, CONN_USB, nullptr);
    HChannel hChannel = new HdcChannel();
    clsServerForClient->AdminChannel(OP_ADD, hChannel->channelId, hChannel);
    server.AttachChannelInnerForUds(hSession, hChannel->channelId);
    EXPECT_TRUE(hChannel->targetSessionId == hSession->sessionId);
#endif
}

}