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

#include "daemon_app.h"
#include "daemon_app_test.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

using namespace testing::ext;

namespace Hdc {
void HdcDaemonAppTest::SetUpTestCase() {}
void HdcDaemonAppTest::TearDownTestCase() {}
void HdcDaemonAppTest::SetUp()
{
#ifdef UT_DEBUG
    Base::SetLogLevel(LOG_ALL);
#else
    Base::SetLogLevel(LOG_OFF);
#endif
}
void HdcDaemonAppTest::TearDown() {}

HWTEST_F(HdcDaemonAppTest, Test_HdcDaemonApp, TestSize.Level0)
{
    HTaskInfo taskInfo = new TaskInformation();

    // case: default struct HTaskInfo with isStableBuf set to true.
    HdcDaemonApp daemonApp1(taskInfo);
    EXPECT_EQ(daemonApp1.commandBegin, CMD_APP_BEGIN);
    EXPECT_EQ(daemonApp1.commandData, CMD_APP_DATA);

    delete taskInfo;
}

HWTEST_F(HdcDaemonAppTest, Test_CommandDispatch, TestSize.Level0)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcDaemonApp daemonApp(taskInfo);
    uint8_t payload = 10;

    // invalid command.
    EXPECT_FALSE(daemonApp.CommandDispatch(CMD_APP_BEGIN, &payload, 10));

    delete taskInfo;
}

HWTEST_F(HdcDaemonAppTest, Test_ReadyForRelease, TestSize.Level0)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcDaemonApp daemonApp(taskInfo);
    daemonApp.refCount = 1;

    // case1: default valid case.
    EXPECT_FALSE(daemonApp.ReadyForRelease());

    delete taskInfo;
}

HWTEST_F(HdcDaemonAppTest, Test_MakeCtxForAppCheck, TestSize.Level3)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcDaemonApp daemonApp(taskInfo);
    daemonApp.ctxNow.master = true;
    daemonApp.ctxNow.transferConfig.optionalName = "test.hap";

    uint8_t payload = 10;
    daemonApp.MakeCtxForAppCheck(&payload, 10);
    EXPECT_FALSE(daemonApp.ctxNow.master);
    EXPECT_EQ(daemonApp.ctxNow.localPath, "/data/local/tmp/test.hap");

    delete taskInfo;
}

/*
 * New tests added below
 */

HWTEST_F(HdcDaemonAppTest, Test_Tar2Dir_NoTar, TestSize.Level0)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcDaemonApp daemonApp(taskInfo);

    // path without ".tar" should return empty string
    string res = daemonApp.Tar2Dir("/tmp/somefile.hap");
    EXPECT_EQ(res, "");

    delete taskInfo;
}

static bool FileExists(const std::string &path)
{
    struct stat st;
    return (lstat(path.c_str(), &st) == 0);
}

HWTEST_F(HdcDaemonAppTest, Test_RemovePath_File, TestSize.Level0)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcDaemonApp daemonApp(taskInfo);

    // create temporary file
    std::string tmpl = "/data/local/tmp/hdc_testfileXXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = mkstemp(buf.data());
    ASSERT_GE(fd, 0);
    close(fd);
    std::string path(buf.data());
    ASSERT_TRUE(FileExists(path));

    // remove using RemovePath
    daemonApp.RemovePath(path);
    EXPECT_FALSE(FileExists(path));

    delete taskInfo;
}

HWTEST_F(HdcDaemonAppTest, Test_RemovePath_DirRecursive, TestSize.Level0)
{
    HTaskInfo taskInfo = new TaskInformation();
    HdcDaemonApp daemonApp(taskInfo);

    // create temporary directory
    char dirtmpl[] = "/data/local/tmp/hdc_testdirXXXXXX";
    char *d = mkdtemp(dirtmpl);
    ASSERT_NE(d, nullptr) << "Failed to create temporary directory";
    std::string dir(d);

    // ensure parent directory exists
    std::string parentDir = "/data/local/tmp";
    if (!FileExists(parentDir)) {
        ASSERT_EQ(mkdir(parentDir.c_str(), 0755), 0) << "Failed to create parent directory";
    }

    // create a nested directory and file
    std::string subdir = dir + "/sub";
    ASSERT_EQ(mkdir(subdir.c_str(), 0755), 0) << "Failed to create directory: " << subdir;
    std::string file1 = dir + "/f1.txt";
    int fd1 = open(file1.c_str(), O_CREAT | O_WRONLY, 0644);
    ASSERT_GE(fd1, 0);
    write(fd1, "x", 1);
    close(fd1);

    std::string file2 = subdir + "/f2.txt";
    int fd2 = open(file2.c_str(), O_CREAT | O_WRONLY, 0644);
    ASSERT_GE(fd2, 0);
    write(fd2, "y", 1);
    close(fd2);

    ASSERT_TRUE(FileExists(file1));
    ASSERT_TRUE(FileExists(file2));

    // remove the directory recursively
    daemonApp.RemovePath(dir);

    EXPECT_FALSE(FileExists(dir));
    EXPECT_FALSE(FileExists(file1));
    EXPECT_FALSE(FileExists(file2));

    delete taskInfo;
}
} // namespace Hdc
