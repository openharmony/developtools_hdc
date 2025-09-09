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

#include "file_ut.h"

using namespace testing::ext;
using namespace testing;

namespace Hdc {

class HdcFileTest : public Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();

    class MockHdcFile : public HdcFile {
    public:
        explicit MockHdcFile(HTaskInfo hTaskInfo) : HdcFile(hTaskInfo) {}
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
        HdcFile *ptrTask = nullptr;
        ptrTask = new (std::nothrow) HdcFile(hTaskInfo);
        if (ptrTask == nullptr) {
            delete hTaskInfo;
            delete unittest;
            return nullptr;
        }
        hTaskInfo->taskClass = ptrTask;
        return hTaskInfo;
    }

    HTaskInfo hTaskInfo;
    MockHdcFile *mockHdcdFile;
};

void HdcFileTest::SetUpTestCase()
{
#ifdef UT_DEBUG
    Hdc::Base::SetLogLevel(LOG_ALL);
#else
    Hdc::Base::SetLogLevel(LOG_OFF);
#endif
}
void HdcFileTest::TearDownTestCase() {}
void HdcFileTest::SetUp()
{
    hTaskInfo = BuildPTask();
    mockHdcdFile = new (std::nothrow) MockHdcFile(hTaskInfo);
}
void HdcFileTest::TearDown()
{
    delete mockHdcdFile;
    mockHdcdFile = nullptr;
}

HWTEST_F(HdcFileTest, TestStopTask, TestSize.Level0)
{
    mockHdcdFile->StopTask();
    ASSERT_EQ(mockHdcdFile->singalStop, true);
}

HWTEST_F(HdcFileTest, TestBegintarnsfer, TestSize.Level0)
{
    HdcFile::CtxFile context;
    // case 1: argc < CMD_ARG1_COUNT
    std::string command = "-cwd";
    bool result = mockHdcdFile->BeginTransfer(&context, command);
    EXPECT_FALSE(result);
    // case 2: argv == nullptr
    command = "";
    result = mockHdcdFile->BeginTransfer(&context, command);
    EXPECT_FALSE(result);
}

HWTEST_F(HdcFileTest, TestCheckBlacklistPath, TestSize.Level0)
{
    HdcFile::CtxFile context;
    // case 1: path not in balcklist
    context.localPath = "/data/service/el1/public/validpath";
    ASSERT_EQ(mockHdcdFile->CheckBlacklistPath(&context), true);

    // case 2: path in balcklist
    context.localPath = "/data/service/el1/public/hdc";
    ASSERT_EQ(mockHdcdFile->CheckBlacklistPath(&context), false);
}

HWTEST_F(HdcFileTest, TestParseMasterParameters, TestSize.Level0)
{
    HdcFile::CtxFile context;
    int argc = 0, srcArgvIndex = 0;
    char arg0[] = "-b";
    char arg1[] = "-z";
    char arg2[] = "-sync";
    char arg3[] = "-a";
    char arg4[] = "-m";
    char *argBase[] = {arg0, arg1, arg2, arg3, arg4};
    char argFirst[] = "-cwd";
    char argRemotePath[] = "D:\\";
    char argBundleName[] = "com.example.myapplication";
    char argLocalPath[] = "data/local/tmp/test.txt";
    // case 1: -b Parameter ok
    argc = 5;
    char *argvB[] = {argFirst, argRemotePath, argBase[0], argBundleName, argLocalPath};
    bool result = mockHdcdFile->ParseMasterParameters(&context, argc, argvB, srcArgvIndex);
    EXPECT_TRUE(result);
    EXPECT_EQ(context.bundleName, "com.example.myapplication");
    EXPECT_EQ(srcArgvIndex, argc - 1);
    // case 2: -b MissingParameter
    argc = 4;
    srcArgvIndex = 0;
    char *argvBInvalid[] = {argFirst, argRemotePath, argBase[0], argLocalPath};
    result = mockHdcdFile->ParseMasterParameters(&context, argc, argvBInvalid, srcArgvIndex);
    EXPECT_FALSE(result);
    EXPECT_EQ(srcArgvIndex, argc);
    int i = 1;
    int numArgs = sizeof(argBase) / sizeof(argBase[0]);
    for (i = 1; i < numArgs; i++)
    {
        argc = 4;
        srcArgvIndex = 0;
        // case: argBase Parameter ok
        char *argv[] = {argFirst, argRemotePath, argBase[i], argLocalPath};
        result = mockHdcdFile->ParseMasterParameters(&context, argc, argv, srcArgvIndex);
        EXPECT_TRUE(result);
        EXPECT_EQ(srcArgvIndex, argc - 1);

        argc = 3;
        srcArgvIndex = 0;
        // case: argBase MissingParameter
        char *argvInvalid[] = {argFirst, argRemotePath, argBase[i]};
        result = mockHdcdFile->ParseMasterParameters(&context, argc, argvInvalid, srcArgvIndex);
        EXPECT_FALSE(result);
        EXPECT_EQ(srcArgvIndex, argc);
    }
}

HWTEST_F(HdcFileTest, TestCheckSandboxSubPath, TestSize.Level0)
{
    HdcFile::CtxFile context;
    // case 1: application path
    string resolvedPath;
    context.bundleName = "com.example.myapplication";
    context.inputLocalPath = "data/storage/el2/base/";
    ASSERT_EQ(mockHdcdFile->CheckSandboxSubPath(&context, resolvedPath), true);
    std::stringstream ss;
    ss << "/mnt/debug/100/debug_hap/com.example.myapplication/data/storage/el2/";
    ASSERT_EQ(resolvedPath, ss.str());
}

HWTEST_F(HdcFileTest, TestSetMasterParameters, TestSize.Level0)
{
    HdcFile::CtxFile context;
    char arg0[] = "-b";
    char arg1[] = "-z";
    char arg2[] = "-sync";
    char arg3[] = "-a";
    char arg4[] = "-m";
    char *argBase[] = {arg0, arg1, arg2, arg3, arg4};
    char argFirst[] = "-cwd";
    char argRemotePath[] = "D:\\";
    char argBundleName[] = "com.example.myapplication";
    char argLocalPath[] = "data/local/tmp/test.txt";
    char argInvalidBundlePath[] = "../../../../test.txt";
    std::filesystem::path testPath = "/mnt/debug/100/debug_hap/com.example.myapplication/data/local/tmp/";
    if (!std::filesystem::exists(testPath))
    {
        std::filesystem::create_directories(testPath);
    }
    ASSERT_TRUE(std::filesystem::exists(testPath));
    // case 1: -b Parameter ok
    int argc = 5;
    context.transferConfig.clientCwd = argRemotePath;
    char *argvB[] = {argFirst, argRemotePath, argBase[0], argBundleName, argLocalPath};
    std::stringstream ss;
    ss << "/mnt/debug/100/debug_hap/com.example.myapplication/data/local/tmp//test.txt";
    EXPECT_EQ(mockHdcdFile->SetMasterParameters(&context, argc, argvB), true);
    EXPECT_EQ(context.bundleName, "com.example.myapplication");
    EXPECT_EQ(context.localPath, ss.str());
    EXPECT_EQ(context.localName, "test.txt");
    // case 2: -b serverOrDaemon == true
    hTaskInfo->serverOrDaemon = true;
    EXPECT_EQ(mockHdcdFile->SetMasterParameters(&context, argc, argvB), false);
    // case 3: -b InvalidBundlePath
    hTaskInfo->serverOrDaemon = false;
    char *argvBInvalidBundlePath[] = {argFirst, argRemotePath, argBase[0], argBundleName, argInvalidBundlePath};
    EXPECT_EQ(mockHdcdFile->SetMasterParameters(&context, argc, argvBInvalidBundlePath), false);
    // case 4: -b MissingParameter
    argc = 4;
    char *argvBInvalid[] = {argFirst, argRemotePath, argBase[0], argLocalPath};
    EXPECT_EQ(mockHdcdFile->SetMasterParameters(&context, argc, argvBInvalid), false);
    context.bundleName = "";
    context.transferConfig.reserve1 = "";

    int i = 1;
    int numArgs = sizeof(argBase) / sizeof(argBase[0]);
    for (i = 1; i < numArgs; i++)
    {
        argc = 4;
        // case: argBase Parameter ok
        char *argv[] = {argFirst, argRemotePath, argBase[i], argLocalPath};
        EXPECT_EQ(mockHdcdFile->SetMasterParameters(&context, argc, argv), true);
        argc = 3;
        // case: argBase MissingParameter
        char *argvInvalid[] = {argFirst, argRemotePath, argBase[i]};
        EXPECT_EQ(mockHdcdFile->SetMasterParameters(&context, argc, argvInvalid), false);
    }
}

HWTEST_F(HdcFileTest, TestIsPathInsideSandbox, TestSize.Level0)
{
    {
        // case 1: 在沙箱路径
        const string path = "data/storage/el2/base/com.example.myapplication/data/file.txt";
        const string appDir = "data/storage/el2/base/com.example.myapplication";
        ASSERT_TRUE(mockHdcdFile->IsPathInsideSandbox(path, appDir));
    }

    {
        // case 2: 不在沙箱路径
        const string path = "data/storage/el2/base/com.example.yourapplication/file.txt";
        const string appDir = "data/storage/el2/base/com.example.myapplication";
        ASSERT_FALSE(mockHdcdFile->IsPathInsideSandbox(path, appDir));
    }

    {
        // case 3: 路径长度小于沙箱路径
        const string path = "data/local/tmp/file.txt";
        const string appDir = "data/storage/el2/base/com.example.myapplication";
        ASSERT_FALSE(mockHdcdFile->IsPathInsideSandbox(path, appDir));
    }
}


HWTEST_F(HdcFileTest, TestSetMasterParametersOnDaemon, TestSize.Level0)
{
    HdcFile::CtxFile context;
    char arg0[] = "-b";
    char argFirst[] = "-cwd";
    char argRemotePath[] = "D:\\";
    char argBundleName[] = "com.example.myapplication";
    char argLocalPath[] = "data/local/tmp/test.txt";
    char argInvalidBundlePath[] = "../../../../test.txt";
    std::filesystem::path testPath = "/mnt/debug/100/debug_hap/com.example.myapplication/data/local/tmp/";
    if (!std::filesystem::exists(testPath))
    {
        std::filesystem::create_directories(testPath);
    }
    ASSERT_TRUE(std::filesystem::exists(testPath));

    {
        // case 1: normal
        int argc = 5;
        context.sandboxMode = true;
        context.transferConfig.reserve1 = "com.example.myapplication";
        char *argvB[] = {argFirst, argRemotePath, arg0, argBundleName, argLocalPath};
        EXPECT_EQ(mockHdcdFile->SetMasterParametersOnDaemon(&context, argc, argvB, argc), true);
    }

    {
        // case 1: Invalid bundle name
        int argc = 5;
        context.sandboxMode = true;
        context.transferConfig.reserve1 = "com.example.test";
        char *argvB[] = {argFirst, argRemotePath, arg0, argBundleName, argInvalidBundlePath};
        EXPECT_EQ(mockHdcdFile->SetMasterParametersOnDaemon(&context, argc, argvB, argc), false);
    }
}

HWTEST_F(HdcFileTest, TestParseAndValidateOptions, TestSize.Level0)
{
    HdcFile::CtxFile ctxNow;
    ctxNow.transferConfig.fileSize = 102400;
    ctxNow.transferConfig.path = "data/local/tmp/file.txt";
    ctxNow.transferConfig.reserve1 = "com.example.myapplication";

    // case 1: application path
    string s = SerialStruct::SerializeToString(ctxNow.transferConfig);
    bool result = mockHdcdFile->ParseAndValidateOptions(reinterpret_cast<uint8_t*>(s.data()), s.size());
    ASSERT_TRUE(result);
}

HWTEST_F(HdcFileTest, TestCheckBundleAndPath, TestSize.Level0)
{
    HdcFile::CtxFile ctxNow;
    hTaskInfo->serverOrDaemon = true;
    bool result = mockHdcdFile->CheckBundleAndPath();
    ASSERT_TRUE(result);
}

}