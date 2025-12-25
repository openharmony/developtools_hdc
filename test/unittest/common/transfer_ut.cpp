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

#include "transfer.h"
#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing::ext;
using namespace testing;

namespace Hdc {

class HdcTransferTest : public Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();

    class MockHdcTransfer : public HdcTransferBase {
    public:
        explicit MockHdcTransfer(HTaskInfo hTaskInfo) : HdcTransferBase(hTaskInfo) {}
    };

private:
    HTaskInfo BuildPTask()
    {
        HTaskInfo hTaskInfo = new(std::nothrow) TaskInformation();
        if (hTaskInfo == nullptr) {
            return nullptr;
        }
        uv_loop_t looptest;
        uv_loop_init(&looptest);
        LoopStatus ls(&looptest, "not support");
        HdcSessionBase *unittest = new (std::nothrow) HdcSessionBase(false);
        if (unittest == nullptr) {
            delete hTaskInfo;
            return nullptr;
        }
        hTaskInfo->ownerSessionClass = unittest;
        hTaskInfo->channelId = 0;
        hTaskInfo->sessionId = 0;
        hTaskInfo->runLoop = &looptest;
        hTaskInfo->runLoopStatus = &ls;
        hTaskInfo->serverOrDaemon = false;
        hTaskInfo->masterSlave = false;
        hTaskInfo->closeRetryCount = 0;
        hTaskInfo->channelTask = false;
        hTaskInfo->isCleared = false;
        hTaskInfo->taskType = TASK_FILE;
        hTaskInfo->hasInitial = true;
        HdcTransferBase *ptrTask = nullptr;
        ptrTask = new (std::nothrow) HdcTransferBase(hTaskInfo);
        if (ptrTask == nullptr) {
            delete hTaskInfo;
            delete unittest;
            return nullptr;
        }
        hTaskInfo->taskClass = ptrTask;
        return hTaskInfo;
    }

    HTaskInfo hTaskInfo;
    MockHdcTransfer *mockHdcdTransfer;
};

void HdcTransferTest::SetUpTestCase()
{
#ifdef UT_DEBUG
    Hdc::Base::SetLogLevel(LOG_ALL);
#else
    Hdc::Base::SetLogLevel(LOG_OFF);
#endif
}
void HdcTransferTest::TearDownTestCase() {}
void HdcTransferTest::SetUp()
{
    hTaskInfo = BuildPTask();
    mockHdcdTransfer = new (std::nothrow) MockHdcTransfer(hTaskInfo);
    mockHdcdTransfer->commandBegin = CMD_FILE_BEGIN;
    mockHdcdTransfer->commandBegin = CMD_FILE_DATA;
    mockHdcdTransfer->isStableBuf = false;
}
void HdcTransferTest::TearDown()
{
    delete mockHdcdTransfer;
    mockHdcdTransfer = nullptr;
}

HWTEST_F(HdcTransferTest, TestCommandDispatch, TestSize.Level0)
{
    uint16_t command = CMD_FILE_DATA;
    uint8_t payload[10];
    int payloadSize = -1;
    ASSERT_EQ(mockHdcdTransfer->CommandDispatch(command, payload, payloadSize), false);
}

HWTEST_F(HdcTransferTest, TestExtractRelativePath, TestSize.Level0)
{
    // normal windows slash
    string cwd = "D:\\";
    string resolvedPath = "test\\path";
    string wantStr = "D:\\test\\path";
    mockHdcdTransfer->ExtractRelativePath(cwd, resolvedPath);
    ASSERT_EQ(resolvedPath, wantStr);

    // normal unix slash
    cwd = "/tmp/";
    resolvedPath = "test/path";
    wantStr = "/tmp/test/path";
    mockHdcdTransfer->ExtractRelativePath(cwd, resolvedPath);
    ASSERT_EQ(resolvedPath, wantStr);

    cwd = "/tmp";
    mockHdcdTransfer->ExtractRelativePath(cwd, resolvedPath);
    ASSERT_EQ(resolvedPath, wantStr);

    // test resolvedPath is absolute path
    cwd = "/tmp/";
    resolvedPath = "/test/path";
    wantStr = "/test/path";
    mockHdcdTransfer->ExtractRelativePath(cwd, resolvedPath);
    ASSERT_EQ(resolvedPath, wantStr);

    // test cwd is empty
    cwd = "";
    resolvedPath = "test/path";
    wantStr = "test/path";
    mockHdcdTransfer->ExtractRelativePath(cwd, resolvedPath);
    ASSERT_EQ(resolvedPath, wantStr);
}

HWTEST_F(HdcTransferTest, TestRemoveSandboxRootPath, TestSize.Level0)
{
    const string bundleName = "com.example.myapplication";
    const string testDir = "data/local/tmp/";
    std::filesystem::path testPath = mockHdcdTransfer->SANDBOX_ROOT_DIR + bundleName + Base::GetPathSep() + testDir;
    if (!std::filesystem::exists(testPath)) {
        std::filesystem::create_directories(testPath);
    }
    ASSERT_TRUE(std::filesystem::exists(testPath));

    // normal test, srcStr contains Sandbox Root Path
    string srcStr = testPath;
    string wantStr = testDir;
    mockHdcdTransfer->RemoveSandboxRootPath(srcStr, bundleName);
    ASSERT_EQ(srcStr, wantStr);

    // srcStr not contains full Sandbox Root Path, missing slash, srcStr.size() < fullPath.size()
    srcStr = mockHdcdTransfer->SANDBOX_ROOT_DIR + bundleName;
    wantStr = srcStr;
    mockHdcdTransfer->RemoveSandboxRootPath(srcStr, bundleName);
    ASSERT_EQ(srcStr, wantStr);

    // srcStr.size() > fullPath.size(), drop first slash
    srcStr = testPath;
    srcStr = srcStr.substr(1);
    wantStr = srcStr;
    mockHdcdTransfer->RemoveSandboxRootPath(srcStr, bundleName);
    ASSERT_EQ(srcStr, wantStr);

    // Invalid bundleName
    const string bundleNameTest = "test-" + bundleName + "-test";
    srcStr = testPath;
    wantStr = srcStr;
    mockHdcdTransfer->RemoveSandboxRootPath(srcStr, bundleNameTest);
    ASSERT_EQ(srcStr, wantStr);
}

HWTEST_F(HdcTransferTest, TestAddFeatures, TestSize.Level0)
{
    union HdcTransferBase::FeatureFlagsUnion f{};
    mockHdcdTransfer->AddFeatures(f);
    ASSERT_EQ(f.bits.reserveBits1, 1);
    ASSERT_EQ(f.bits.hugeBuf, true);
}

HWTEST_F(HdcTransferTest, TestCheckFeatures, TestSize.Level0)
{
    uint8_t payload[FEATURE_FLAG_MAX_SIZE] = {1};
    // case 1: payloadSize < 0;
    int payloadSize = 0;
    HdcTransferBase::CtxFile context;
    bool result = mockHdcdTransfer->CheckFeatures(&context, payload, payloadSize);
    EXPECT_TRUE(result);
    ASSERT_EQ(context.isStableBufSize, true);
    ASSERT_EQ(context.isOtherSideSandboxSupported, false);
    // case 1: payloadSize = FEATURE_FLAG_MAX_SIZE;
    payloadSize = FEATURE_FLAG_MAX_SIZE;
    result = mockHdcdTransfer->CheckFeatures(&context, payload, payloadSize);
    EXPECT_TRUE(result);
    ASSERT_EQ(context.isStableBufSize, false);
    ASSERT_EQ(context.isOtherSideSandboxSupported, false);
    // case 1: other payloadSize
    payloadSize = FEATURE_FLAG_MAX_SIZE + 1;
    result = mockHdcdTransfer->CheckFeatures(&context, payload, payloadSize);
    EXPECT_FALSE(result);
}

HWTEST_F(HdcTransferTest, TestCheckSandboxOptionCompatibility, TestSize.Level0)
{
    HdcTransferBase::CtxFile context;
    bool result = mockHdcdTransfer->CheckSandboxOptionCompatibility(mockHdcdTransfer->cmdBundleName, &context);
    EXPECT_TRUE(result);

    context.isOtherSideSandboxSupported = false;
    ASSERT_EQ(mockHdcdTransfer->CheckSandboxOptionCompatibility(mockHdcdTransfer->cmdBundleName, &context), true);
}

HWTEST_F(HdcTransferTest, TestResetCtx, TestSize.Level0)
{
    HdcTransferBase::CtxFile context;
    ASSERT_EQ(mockHdcdTransfer->ResetCtx(&context, true), true);
    ASSERT_EQ(context.openFd, -1);
}

HWTEST_F(HdcTransferTest, TestSimpleFileIO, TestSize.Level0)
{
    HdcTransferBase::CtxFile context;
    int bufLen = MAX_SIZE_IOBUF_STABLE * mockHdcdTransfer->maxTransferBufFactor;
    context.indexIO = rand();
    context.isStableBufSize = true;
    int result = mockHdcdTransfer->SimpleFileIO(&context, context.indexIO, nullptr, bufLen + 1);
    ASSERT_EQ(result, -1);
    result = mockHdcdTransfer->SimpleFileIO(&context, context.indexIO, nullptr, -1);
    ASSERT_EQ(result, -1);

    context.ioFinish = true;
    result = mockHdcdTransfer->SimpleFileIO(&context, context.indexIO, nullptr, bufLen);
    ASSERT_EQ(result, -1);

    // case: memcpy error
    context.ioFinish = false;
    context.master = false;
    result = mockHdcdTransfer->SimpleFileIO(&context, context.indexIO, nullptr, bufLen);
    ASSERT_EQ(result, -1);
}

HWTEST_F(HdcTransferTest, TestInitTransferPayload, TestSize.Level0)
{
    HdcTransferBase::TransferPayload payloadHead;
    uint64_t index = rand();
    uint8_t compressType = HdcTransferBase::COMPRESS_LZ4;
    bool result = mockHdcdTransfer->InitTransferPayload(payloadHead, index,
                                                        compressType, MAX_SIZE_IOBUF_STABLE);

    ASSERT_EQ(result, true);
    ASSERT_EQ(payloadHead.compressType, compressType);
    ASSERT_EQ(payloadHead.uncompressSize, MAX_SIZE_IOBUF_STABLE);
    ASSERT_EQ(payloadHead.index, index);
}

HWTEST_F(HdcTransferTest, TestSendIOPayload, TestSize.Level0)
{
    HdcTransferBase::CtxFile context;
    uint64_t index = rand();
    uint8_t *payload = new uint8_t[MAX_SIZE_IOBUF_STABLE];
    memset_s(payload, MAX_SIZE_IOBUF_STABLE, 1, MAX_SIZE_IOBUF_STABLE);

    bool result = mockHdcdTransfer->SendIOPayload(&context, index,
                                                    payload, -1);
    ASSERT_EQ(result, false);
}

HWTEST_F(HdcTransferTest, TestProcressFileIOWrite, TestSize.Level0)
{
    HdcTransferBase::CtxFile context;
    uv_fs_t req;
    req.result = 0;
    context.indexIO = context.fileSize = rand();
    bool result = mockHdcdTransfer->ProcressFileIOWrite(&req, &context,
                                                        (HdcTransferBase *)context.thisClass);
    ASSERT_EQ(result, true);

    req.result = -1;
    context.indexIO = 1;
    context.fileSize = 2;
    result = mockHdcdTransfer->ProcressFileIOWrite(&req, &context,
                                                    (HdcTransferBase *)context.thisClass);
    ASSERT_EQ(result, false);
}

HWTEST_F(HdcTransferTest, TestProcressFileIO, TestSize.Level0)
{
    HdcTransferBase::CtxFile context;
    uint64_t bytes = rand();
    uv_fs_t req;
    req.result = -1;

    context.ioFinish = true;
    bool result = mockHdcdTransfer->ProcressFileIO(&req, &context,
                                                    (HdcTransferBase *)context.thisClass, bytes);
    ASSERT_EQ(result, true);

    context.ioFinish = false;
    context.master = true;
    result = mockHdcdTransfer->ProcressFileIO(&req, &context,
                                                (HdcTransferBase *)context.thisClass, bytes);
    ASSERT_EQ(result, true);

    req.result = 0;
    req.fs_type = UV_FS_WRITE;
    result = mockHdcdTransfer->ProcressFileIO(&req, &context,
                                                (HdcTransferBase *)context.thisClass, bytes);
    ASSERT_EQ(result, true);
}

HWTEST_F(HdcTransferTest, TestIODelayed, TestSize.Level0)
{
    HdcTransferBase::CtxFile context;
    uint64_t bytes = rand();
    HdcTransferBase::CtxFileIO *ioContext = new (std::nothrow) HdcTransferBase::CtxFileIO();
    uint8_t *buf = new uint8_t[bytes + HdcTransferBase::payloadPrefixReserve]();
    uv_fs_t *req = &ioContext->fs;
    ioContext->bytes = bytes;
    ioContext->bufIO = buf + HdcTransferBase::payloadPrefixReserve;
    ioContext->context = &context;
    req->data = ioContext;
    req->fs_type = UV_FS_WRITE;
    bool result = mockHdcdTransfer->IODelayed(req);
    ASSERT_EQ(result, false);
}

HWTEST_F(HdcTransferTest, TestIsValidBundlePath, TestSize.Level0)
{
    std::filesystem::path testPath = "/mnt/debug/100/debug_hap/com.example.myapplication/data/local/tmp/";
    if (!std::filesystem::exists(testPath))
    {
        std::filesystem::create_directories(testPath);
    }
    ASSERT_TRUE(std::filesystem::exists(testPath));
    const string bundleName = "com.example.myapplication";
    bool result = mockHdcdTransfer->IsValidBundlePath(bundleName);
    ASSERT_EQ(result, true);
}

HWTEST_F(HdcTransferTest, TestMatchPackageExtendName, TestSize.Level0)
{
    string resolvedPath = "D:\\test\\path\\libA_v10001.hsp";
    bool result = mockHdcdTransfer->MatchPackageExtendName(resolvedPath, ".hsp");
    ASSERT_EQ(result, true);

    string invalidPath = "D:\\test\\path\\libA_v10001.hp";
    result = mockHdcdTransfer->MatchPackageExtendName(invalidPath, ".hsp");
    ASSERT_EQ(result, false);
}

HWTEST_F(HdcTransferTest, TestRecvIOPayload, TestSize.Level0)
{
    HdcTransferBase::CtxFile context;
    uint8_t *payload = new uint8_t[BUF_SIZE_TINY];
    memset_s(payload, BUF_SIZE_TINY, 1, BUF_SIZE_TINY);

    bool result = mockHdcdTransfer->RecvIOPayload(&context, payload, -1);
    ASSERT_EQ(result, false);
}

}
