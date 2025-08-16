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
#include "host_fport_listen_ip_test.h"
#ifdef HDC_HOST
#include "forward.h"
#include "uv.h"
#include <securec.h>
#include "define.h"
#include "define_enum.h"
#endif

#include <stdio.h>

using namespace testing::ext;

namespace Hdc {
void HdcHostFportListenIpTest::SetUpTestCase() {}
void HdcHostFportListenIpTest::TearDownTestCase() {}
void HdcHostFportListenIpTest::SetUp()
{
}
void HdcHostFportListenIpTest::TearDown() {}


HWTEST_F(HdcHostFportListenIpTest, Test_SetupTcpListenAllIp1, TestSize.Level0)
{
#ifdef HDC_HOST
    HTaskInfo taskInfo = new(std::nothrow) TaskInformation();
    HdcForwardBase forward(taskInfo);
    HdcForwardBase::HCtxForward ctxPoint = (HdcForwardBase::HCtxForward)forward.MallocContext(true);
    string forwardListenIP = IPV4_MAPPING_PREFIX;
    forwardListenIP += "127.0.0.1";
    HdcForwardBase::SetForwardListenIP(forwardListenIP);

    ctxPoint->tcp.data = ctxPoint;
    uv_loop_t loopTask;
    uv_loop_init(&loopTask);
    uv_tcp_init(&loopTask, &ctxPoint->tcp);

    int rc = -1;
    bool ret = forward.SetupTcpListenAllIp(ctxPoint, 9988, rc);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(rc == 0);
#endif
}

HWTEST_F(HdcHostFportListenIpTest, Test_SetupTcpListenAllIp2, TestSize.Level0)
{
#ifdef HDC_HOST
    HTaskInfo taskInfo = new(std::nothrow) TaskInformation();
    HdcForwardBase forward(taskInfo);
    HdcForwardBase::HCtxForward ctxPoint = (HdcForwardBase::HCtxForward)forward.MallocContext(true);
    string forwardListenIP = IPV4_MAPPING_PREFIX;
    forwardListenIP += "22.121.120.15";
    HdcForwardBase::SetForwardListenIP(forwardListenIP);

    ctxPoint->tcp.data = ctxPoint;
    uv_loop_t loopTask;
    uv_loop_init(&loopTask);
    uv_tcp_init(&loopTask, &ctxPoint->tcp);

    int rc = -1;
    bool ret = forward.SetupTcpListenAllIp(ctxPoint, 9987, rc);
    EXPECT_TRUE(!ret);
    EXPECT_TRUE(rc != 0);
#endif
}

}