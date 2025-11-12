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
#ifndef HDC_JDWP_TEST_H
#define HDC_JDWP_TEST_H

#include <gtest/gtest.h>
namespace Hdc {
class HdcJdwpTest : public ::testing::Test {
public:
    HdcJdwpTest() {}
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();

    uv_loop_t mLoop;
    uv_loop_t* loop;
    LoopStatus* loopStatus;
    HdcJdwp* jdwp;
    uv_idle_t mIdle;
    int mCycles;
    int mCallDuration = 100; // 默认值为 100 毫秒
};
} // namespace Hdc

#endif // HDC_JDWP_TEST_H