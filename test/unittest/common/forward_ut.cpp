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
#include <gtest/gtest.h>
#include "forward.h"
#include "task.h"

using namespace testing::ext;
using namespace testing;

namespace Hdc {
class HdcForwardBaseTest : public ::testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void HdcForwardBaseTest::SetUpTestCase() {}
void HdcForwardBaseTest::TearDownTestCase() {}
void HdcForwardBaseTest::SetUp() {}
void HdcForwardBaseTest::TearDown() {}

HWTEST_F(HdcForwardBaseTest, HdcForwardBase_ConstructorInitializesFdsToMinusOne, TestSize.Level0)
{
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_EQ(forward.fds[0], -1);
    EXPECT_EQ(forward.fds[1], -1);

    delete info;
}

HWTEST_F(HdcForwardBaseTest, StopTask_WithTwoEntries, TestSize.Level0)
{
    auto* ctx1 = new HdcForwardBase::ContextForward();
    auto* ctx2 = new HdcForwardBase::ContextForward();
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);

    ctx1->id = 1;
    ctx1->type = HdcForwardBase::FORWARD_TCP;
    ctx1->thisClass = &forward;
    ctx1->localArgs[0] = "tcp";
    ctx1->localArgs[1] = "8080";
    ctx1->remoteArgs[0] = "tcp";
    ctx1->remoteArgs[1] = "remote:9090";
    ctx1->tcp.data = ctx1;  // 设置 data 为自身，供 uv_close 使用
    ctx1->finish = true;

    ctx2->id = 2;
    ctx2->type = HdcForwardBase::FORWARD_ABSTRACT;
    ctx2->thisClass = &forward;
    ctx2->localArgs[0] = "localabstract";
    ctx2->localArgs[1] = "sockname";
    ctx2->pipe.data = ctx2;
    ctx2->finish = true;

    forward.mapCtxPoint[1] = ctx1;
    forward.mapCtxPoint[2] = ctx2;

    EXPECT_FALSE(forward.mapCtxPoint.empty());
    forward.StopTask();
    EXPECT_TRUE(forward.mapCtxPoint.empty());

    delete ctx1;
    delete ctx2;
    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_ValidTcp, TestSize.Level0)
{
    std::string as[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_TRUE(forward.CheckNodeInfo("tcp:1234", as));
    EXPECT_EQ(as[0], "tcp");
    EXPECT_EQ(as[1], "1234");

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_InvalidFormat, TestSize.Level0)
{
    std::string as[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.CheckNodeInfo("tcp:", as));
    EXPECT_FALSE(forward.CheckNodeInfo(":1234", as));
    EXPECT_FALSE(forward.CheckNodeInfo("tcp", as));
    EXPECT_FALSE(forward.CheckNodeInfo("", as));

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_InvalidPort, TestSize.Level0)
{
    std::string as[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.CheckNodeInfo("tcp:0", as));
    EXPECT_FALSE(forward.CheckNodeInfo("tcp:999999", as));

    delete info;
}

HWTEST_F(HdcForwardBaseTest, AllocForwardBuf_SizeExceedsMaxBulk, TestSize.Level0)
{
    size_t sizeSuggested = MAX_USBFFS_BULK + 1024;
    uv_handle_t dummyHandle;
    uv_buf_t buf;
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);

    forward.AllocForwardBuf(&dummyHandle, sizeSuggested, &buf);

    ASSERT_NE(buf.base, nullptr);
    EXPECT_EQ(buf.len, MAX_USBFFS_BULK - 1);

    delete info;
    delete[] buf.base;
}

HWTEST_F(HdcForwardBaseTest, AllocForwardBuf_NormalSize, TestSize.Level0)
{
    size_t sizeSuggested = 1024;
    uv_handle_t dummyHandle;
    uv_buf_t buf;
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);

    forward.AllocForwardBuf(&dummyHandle, sizeSuggested, &buf);

    ASSERT_NE(buf.base, nullptr);
    EXPECT_EQ(buf.len, sizeSuggested - 1);

    delete info;
    delete[] buf.base;
}

HWTEST_F(HdcForwardBaseTest, AllocForwardBuf_ZeroSize, TestSize.Level0)
{
    size_t sizeSuggested = 0;
    uv_handle_t dummyHandle;
    uv_buf_t buf;
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);

    forward.AllocForwardBuf(&dummyHandle, sizeSuggested, &buf);

    ASSERT_NE(buf.base, nullptr);
    EXPECT_EQ(buf.len, 0);

    delete info;
    delete[] buf.base;
}

HWTEST_F(HdcForwardBaseTest, AllocForwardBuf_ExactMaxBulkSize, TestSize.Level0)
{
    size_t sizeSuggested = MAX_USBFFS_BULK;
    uv_handle_t dummyHandle;
    uv_buf_t buf;
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);

    forward.AllocForwardBuf(&dummyHandle, sizeSuggested, &buf);

    ASSERT_NE(buf.base, nullptr);
    EXPECT_EQ(buf.len, MAX_USBFFS_BULK - 1);
    
    delete info;
    delete[] buf.base;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_NullInput, TestSize.Level0)
{
    string result[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.CheckNodeInfo(nullptr, result));

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_EmptyString, TestSize.Level0)
{
    string result[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.CheckNodeInfo("", result));

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_ColonAtStart, TestSize.Level0)
{
    string result[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.CheckNodeInfo(":8080", result));

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_ColonAtEnd, TestSize.Level0)
{
    string result[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.CheckNodeInfo("tcp:", result));

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_NoColon, TestSize.Level0)
{
    string result[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.CheckNodeInfo("tcp", result));

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_TcpWithInvalidPortText, TestSize.Level0)
{
    string result[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.CheckNodeInfo("tcp:abc", result));

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_TcpWithZeroPort, TestSize.Level0)
{
    string result[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.CheckNodeInfo("tcp:0", result));

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_TcpWithPortExceedingMax, TestSize.Level0)
{
    string result[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.CheckNodeInfo("tcp:65536", result)); // MAX_IP_PORT = 65535

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_TcpWithValidPort, TestSize.Level0)
{
    string result[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_TRUE(forward.CheckNodeInfo("tcp:8080", result));
    EXPECT_EQ(result[0], "tcp");
    EXPECT_EQ(result[1], "8080");

    delete info;
}

HWTEST_F(HdcForwardBaseTest, CheckNodeInfo_NonTcpWithValidFormat, TestSize.Level0)
{
    string result[2];
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_TRUE(forward.CheckNodeInfo("udp:192.168.1.1", result));
    EXPECT_EQ(result[0], "udp");
    EXPECT_EQ(result[1], "192.168.1.1");

    delete info;
}

HWTEST_F(HdcForwardBaseTest, DetechForwardType_TC01_TCP, TestSize.Level0)
{
    auto* ctx = new HdcForwardBase::ContextForward();
    ctx->localArgs[0] = "tcp";
    ctx->localArgs[1] = "any";
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_TRUE(forward.DetechForwardType(ctx));
    EXPECT_EQ(ctx->type, HdcForwardBase::FORWARD_TCP);

    delete info;
    delete ctx;
}

HWTEST_F(HdcForwardBaseTest, DetectForwardType_TC02_DEV, TestSize.Level0)
{
    auto* ctx = new HdcForwardBase::ContextForward();
    ctx->localArgs[0] = "dev";
    ctx->localArgs[1] = "any";
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_TRUE(forward.DetechForwardType(ctx));
    EXPECT_EQ(ctx->type, HdcForwardBase::FORWARD_DEVICE);

    delete info;
    delete ctx;
}

HWTEST_F(HdcForwardBaseTest, DetectForwardType_TC03_LOCALABSTRACT, TestSize.Level0)
{
    auto* ctx = new HdcForwardBase::ContextForward();
    ctx->localArgs[0] = "localabstract";
    ctx->localArgs[1] = "any";
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_TRUE(forward.DetechForwardType(ctx));
    EXPECT_EQ(ctx->type, HdcForwardBase::FORWARD_ABSTRACT);

    delete info;
    delete ctx;
}

HWTEST_F(HdcForwardBaseTest, DetectForwardType_TC04_LOCALRESERVED, TestSize.Level0)
{
    auto* ctx = new HdcForwardBase::ContextForward();
    ctx->localArgs[0] = "localreserved";
    ctx->localArgs[1] = "sockname";
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_TRUE(forward.DetechForwardType(ctx));
    EXPECT_EQ(ctx->type, HdcForwardBase::FORWARD_RESERVED);
    EXPECT_EQ(ctx->localArgs[1], "/dev/socket/sockname");

    delete info;
    delete ctx;
}

HWTEST_F(HdcForwardBaseTest, DetectForwardType_TC05_LOCALFILESYSTEM, TestSize.Level0)
{
    auto* ctx = new HdcForwardBase::ContextForward();
    ctx->localArgs[0] = "localfilesystem";
    ctx->localArgs[1] = "path";
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_TRUE(forward.DetechForwardType(ctx));
    EXPECT_EQ(ctx->type, HdcForwardBase::FORWARD_FILESYSTEM);
    EXPECT_EQ(ctx->localArgs[1], "/tmp/path");

    delete info;
    delete ctx;
}

HWTEST_F(HdcForwardBaseTest, DetectForwardType_TC06_JDWP, TestSize.Level0)
{
    auto* ctx = new HdcForwardBase::ContextForward();
    ctx->localArgs[0] = "jdwp";
    ctx->localArgs[1] = "any";
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_TRUE(forward.DetechForwardType(ctx));
    EXPECT_EQ(ctx->type, HdcForwardBase::FORWARD_JDWP);

    delete info;
    delete ctx;
}

HWTEST_F(HdcForwardBaseTest, DetectForwardType_TC07_ARK, TestSize.Level0)
{
    auto* ctx = new HdcForwardBase::ContextForward();
    ctx->localArgs[0] = "ark";
    ctx->localArgs[1] = "any";
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_TRUE(forward.DetechForwardType(ctx));
    EXPECT_EQ(ctx->type, HdcForwardBase::FORWARD_ARK);

    delete info;
    delete ctx;
}

HWTEST_F(HdcForwardBaseTest, DetectForwardType_TC08_INVALID, TestSize.Level0)
{
    auto* ctx = new HdcForwardBase::ContextForward();
    ctx->localArgs[0] = "invalid";
    ctx->localArgs[1] = "any";
    HTaskInfo info = new TaskInformation();
    HdcForwardBase forward(info);
    EXPECT_FALSE(forward.DetechForwardType(ctx));

    delete info;
    delete ctx;
}

} // namespace Hdc