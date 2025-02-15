/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "heartbeat_ut.h"
#include "securec.h"
#include "serial_struct.h"

using namespace testing::ext;
namespace Hdc {
void HdcHeartbeatTest::SetUpTestCase() {}
void HdcHeartbeatTest::TearDownTestCase() {}
void HdcHeartbeatTest::SetUp() {}
void HdcHeartbeatTest::TearDown() {}

HWTEST_F(HdcHeartbeatTest, TestAddHeartbeatCount, TestSize.Level0)
{
    HdcHeartbeat heartbeat;
    int count = heartbeat.heartbeatCount;
    heartbeat.AddHeartbeatCount();
    ASSERT_EQ(heartbeat.heartbeatCount, count + 1);
}

HWTEST_F(HdcHeartbeatTest, TestAddMessageCount, TestSize.Level0)
{
    HdcHeartbeat heartbeat;
    int count = heartbeat.messageCount;
    heartbeat.AddMessageCount();
    ASSERT_EQ(heartbeat.messageCount, count + 1);
}

HWTEST_F(HdcHeartbeatTest, TestGetHeartbeatCount, TestSize.Level0)
{
    HdcHeartbeat heartbeat;
    int count = heartbeat.heartbeatCount;
    ASSERT_EQ(heartbeat.GetHeartbeatCount(), count);
}

HWTEST_F(HdcHeartbeatTest, TestToString, TestSize.Level0)
{
    HdcHeartbeat heartbeat;
    std::string str = heartbeat.ToString();
    std::stringstream ss;
    ss << "heartbeat count is " << heartbeat.heartbeatCount << " and messages count is " << heartbeat.messageCount;
    ASSERT_EQ(str, ss.str());
}

HWTEST_F(HdcHeartbeatTest, TestHandleRecvHeartbeatMsg, TestSize.Level0)
{
    HdcSessionBase::HeartbeatMsg heartbeatMsg;
    HdcHeartbeat heartbeat;
    int count = rand();
    heartbeatMsg.heartbeatCount = count;
    std::string s = SerialStruct::SerializeToString(heartbeatMsg);
    std::string str = heartbeat.HandleRecvHeartbeatMsg(
        reinterpret_cast<uint8_t *>(const_cast<char *>(s.c_str())), s.size());
    std::stringstream ss;
    ss << "heartbeat count is " << count;
    ASSERT_EQ(str, ss.str());
}

HWTEST_F(HdcHeartbeatTest, TestSupportHeartbeat, TestSize.Level0)
{
    HdcHeartbeat heartbeat;
    bool supportHeartbeat = true;
    heartbeat.SetSupportHeartbeat(supportHeartbeat);
    ASSERT_EQ(heartbeat.supportHeartbeat, supportHeartbeat);
    ASSERT_EQ(heartbeat.GetSupportHeartbeat(), supportHeartbeat);
}
}