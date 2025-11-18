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

#include "jdwp.h"
#include "uv_status.h"
#include "jdwp_test.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <uv.h>

using namespace testing::ext;

namespace Hdc {

void HdcJdwpTest::SetUpTestCase() { }

void HdcJdwpTest::TearDownTestCase() {}

void HdcJdwpTest::SetUp()
{
    uv_loop_init(&mLoop);
    loopStatus = new LoopStatus(&mLoop, "HandleTestLoop");
    loop = uv_default_loop();
    jdwp = new HdcJdwp(loop, loopStatus);
}

void HdcJdwpTest::TearDown()
{
    delete jdwp;
    delete loopStatus;
    loopStatus = nullptr;
}

HWTEST_F(HdcJdwpTest, Test_CheckPIDExist_ValidPID, TestSize.Level0)
{
    uint32_t validPID = 12345;
    HdcJdwp::HCtxJdwp ctx = static_cast<HdcJdwp::HCtxJdwp>(jdwp->MallocContext());
    ctx->pid = validPID;
    jdwp->AdminContext(OP_ADD, validPID, ctx);

    EXPECT_TRUE(jdwp->CheckPIDExist(validPID));

    jdwp->AdminContext(OP_REMOVE, validPID, nullptr);
    jdwp->FreeContext(ctx);
}

HWTEST_F(HdcJdwpTest, Test_CheckPIDExist_InvalidPID, TestSize.Level0)
{
    uint32_t invalidPID = 5678;
    EXPECT_FALSE(jdwp->CheckPIDExist(invalidPID));
}

HWTEST_F(HdcJdwpTest, Test_AdminContext_AddAndQuery, TestSize.Level0)
{
    uint32_t pid = 9999;
    HdcJdwp::HCtxJdwp ctx = static_cast<HdcJdwp::HCtxJdwp>(jdwp->MallocContext());
    ctx->pid = pid;
    jdwp->AdminContext(OP_ADD, pid, ctx);

    HdcJdwp::HCtxJdwp queriedCtx = static_cast<HdcJdwp::HCtxJdwp>(jdwp->AdminContext(OP_QUERY, pid, nullptr));
    EXPECT_EQ(queriedCtx, ctx);
    EXPECT_EQ(queriedCtx->pid, pid);

    jdwp->AdminContext(OP_REMOVE, pid, nullptr);
    jdwp->FreeContext(ctx);
}

HWTEST_F(HdcJdwpTest, Test_AdminContext_Remove, TestSize.Level0)
{
    uint32_t pid = 8888;
    HdcJdwp::HCtxJdwp ctx = static_cast<HdcJdwp::HCtxJdwp>(jdwp->MallocContext());
    ctx->pid = pid;
    jdwp->AdminContext(OP_ADD, pid, ctx);

    jdwp->AdminContext(OP_REMOVE, pid, nullptr);

    EXPECT_FALSE(jdwp->CheckPIDExist(pid));

    jdwp->FreeContext(ctx);
}

HWTEST_F(HdcJdwpTest, Test_ReadyForRelease, TestSize.Level0)
{
    EXPECT_TRUE(jdwp->ReadyForRelease());

    HdcJdwp::HCtxJdwp ctx = static_cast<HdcJdwp::HCtxJdwp>(jdwp->MallocContext());
    jdwp->AdminContext(OP_ADD, 12345, ctx);

    EXPECT_FALSE(jdwp->ReadyForRelease());

    jdwp->AdminContext(OP_REMOVE, 12345, nullptr);
    jdwp->FreeContext(ctx);
    if (jdwp->refCount > 0) {
        --jdwp->refCount;
    }

    EXPECT_TRUE(jdwp->ReadyForRelease());
}

HWTEST_F(HdcJdwpTest, Test_SendJdwpNewFD, TestSize.Level0)
{
    uint32_t targetPID = 12345;
    int fd = 10;

    HdcJdwp::HCtxJdwp ctx = static_cast<HdcJdwp::HCtxJdwp>(jdwp->MallocContext());
    ctx->pid = targetPID;
    jdwp->AdminContext(OP_ADD, targetPID, ctx);

    EXPECT_FALSE(jdwp->SendJdwpNewFD(targetPID, fd));

    jdwp->AdminContext(OP_REMOVE, targetPID, nullptr);
    if (jdwp->refCount > 0) {
        --jdwp->refCount;
    }
    jdwp->FreeContext(ctx);
}

HWTEST_F(HdcJdwpTest, Test_SendArkNewFD, TestSize.Level0)
{
    std::string arkStr = "ark:12345@tid@Debugger";
    int fd = 10;

    uint32_t pid = 12345;
    HdcJdwp::HCtxJdwp ctx = static_cast<HdcJdwp::HCtxJdwp>(jdwp->MallocContext());
    ctx->pid = pid;
    jdwp->AdminContext(OP_ADD, pid, ctx);

    EXPECT_TRUE(jdwp->SendArkNewFD(arkStr, fd));

    jdwp->AdminContext(OP_REMOVE, pid, nullptr);
    if (jdwp->refCount > 0) {
        --jdwp->refCount;
    }
    jdwp->FreeContext(ctx);
}

HWTEST_F(HdcJdwpTest, Test_FdEventPollThread, TestSize.Level0)
{
    EXPECT_EQ(jdwp->CreateFdEventPoll(), RET_SUCCESS);
}

} // namespace Hdc