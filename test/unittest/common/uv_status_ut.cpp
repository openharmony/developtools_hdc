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

#include "log.h"
#include "uv_status.h"
#include <random>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing::ext;
using namespace testing;

namespace Hdc {
class HandleTest : public testing::Test {
public:
    HandleTest() {}
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
public:
    uv_loop_t mLoop;
    uv_idle_t mIdle;
    LoopStatus *mLoopStatus;
    unsigned int mCallDuration;
    int mCycles;
};

static void IdleCallBack(uv_idle_t* handle)
{
    HandleTest *ht = (HandleTest *)handle->data;
    static int cycles = 0;
    CALLSTAT_GUARD(*(ht->mLoopStatus), &(ht->mLoop), "IdleCallBack");

    uv_sleep(ht->mCallDuration);
    if (cycles++ > ht->mCycles) {
        uv_stop(&(ht->mLoop));
    }
}

void HandleTest::SetUpTestCase() { }

void HandleTest::TearDownTestCase() {}

void HandleTest::SetUp()
{
    mCallDuration = 1 * MS_PER_SEC;
    uv_loop_init(&mLoop);
    mIdle.data = this;
    uv_idle_init(&mLoop, &mIdle);
    uv_idle_start(&mIdle, IdleCallBack);
    StartLoopMonitor();
    mLoopStatus = new LoopStatus(&mLoop, "HandleTestLoop");
}

void HandleTest::TearDown()
{
    delete mLoopStatus;
    mLoopStatus = nullptr;
}

HWTEST_F(HandleTest, Handle_NoHungTest_001, TestSize.Level0)
{
    mCycles = 5;
    mCallDuration = MS_PER_SEC / 2;
    uv_run(&mLoop, UV_RUN_DEFAULT);
    ASSERT_EQ(0, 0);
}
HWTEST_F(HandleTest, Handle_HungTest_001, TestSize.Level0)
{
    mCycles = 1;
    mCallDuration = 6 * MS_PER_SEC;
    uv_run(&mLoop, UV_RUN_DEFAULT);
    ASSERT_EQ(0, 0);
}

} // namespace Hdc
