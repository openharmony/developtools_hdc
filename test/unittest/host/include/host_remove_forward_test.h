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
#ifndef HDC_HOST_REMOVE_FORWARD_TEST_H
#define HDC_HOST_REMOVE_FORWARD_TEST_H
#include <gtest/gtest.h>
#include "uv.h"
#include "server.h"
#include "server_for_client.h"

namespace Hdc {
class HdcHostRemoveForwardTest : public ::testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
private:
    void AddForward(uint32_t sessionId);

    uv_loop_t loopMain;
    HdcServer *server;
    HdcServerForClient *cls;
};
} // namespace Hdc
#endif // HDC_HOST_REMOVE_FORWARD_TEST_H
