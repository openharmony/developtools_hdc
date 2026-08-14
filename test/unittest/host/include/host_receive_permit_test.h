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
#ifndef HDC_HOST_RECEIVE_PERMIT_TEST_H
#define HDC_HOST_RECEIVE_PERMIT_TEST_H

#include <gtest/gtest.h>

#include "server.h"
#include "server_for_client.h"

namespace Hdc {
class HdcHostReceivePermitTest : public ::testing::Test {
public:
    void SetUp();
    void TearDown();

protected:
    HChannel CreateChannel(uint32_t channelId, uint32_t sessionId);
    std::string SerializeFileConfig(const std::string &cwd, const std::string &path,
        const std::string &optionalName = "");
    void InitWritableProxy(HChannel channel, uv_tcp_t &peer);
    void CloseWritableProxy(HChannel channel, uv_tcp_t &peer);

    uv_loop_t loopMain;
    HdcServer *server = nullptr;
    HdcServerForClient *serverForClient = nullptr;
};
} // namespace Hdc

#endif // HDC_HOST_RECEIVE_PERMIT_TEST_H
