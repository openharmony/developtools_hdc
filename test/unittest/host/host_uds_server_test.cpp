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
#endif

#include <stdio.h>

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
    printf("0000\n");
#ifdef __OHOS__
    printf("1111");
    uv_loop_t loopMain;
    uv_loop_init(&loopMain);
    printf("222");
    HdcServerForClient *cls = new HdcServerForClient(true, std::string("uds"), nullptr, &loopMain);
    printf("333");
    uv_pipe_t udsListen;
    udsListen.data = cls;
    printf("4444");
    HdcServerForClient::AcceptUdsClient((uv_stream_t *)&udsListen, 0);
    printf("55555");

    delete cls;
#endif
}

}