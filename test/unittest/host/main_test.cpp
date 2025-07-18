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
#ifdef __OHOS__
#include <sys/xattr.h>
#include <sys/wait.h>
#endif
#include "base.h"
#include "main_test.h"

using namespace testing::ext;

namespace Hdc {
void HdcMainTest::SetUpTestCase() {}
void HdcMainTest::TearDownTestCase() {}
void HdcMainTest::SetUp()
{
#ifdef UT_DEBUG
    Base::SetLogLevel(LOG_ALL);
#else
    Base::SetLogLevel(LOG_OFF);
#endif
}
void HdcMainTest::TearDown() {}

bool HdcMainTest::TestIsHishellLabel(pid_t pid)
{
    if (pid == 0) {
        return false;
    }
    // static path
    std::stringstream ss;
    ss << "/proc/" << pid << "/attr/current";
    std::string cmd = ss.str();
    const char *pathBuf = cmd.c_str();
    const char* attrName = "security.selinux";
    // get attribute size
    ssize_t attrSize = getxattr(pathBuf, attrName, nullptr, 0);
    if (attrSize == 0 || attrSize == - 1) {
        return false;
    }
    char *attrValue = new(std::nothrow) char[attrSize];
    if (attrValue == nullptr) {
        return false;
    }
    // get attribute value
    if (getxattr(pathBuf, attrName, attrValue, attrSize) == -1) {
        delete []attrValue;
        return false;
    }
    string label(attrValue, attrSize - 1);
    delete []attrValue;
    return label == "u:r:hishell_hap:s0";
}

HWTEST_F(HdcMainTest, TestIsHishell_1, TestSize.Level0)
{
    EXPECT_FALSE(TestIsHishellLabel(0));
}

HWTEST_F(HdcMainTest, TestIsHishell_2, TestSize.Level0)
{
    pid_t pid = fork();
    ASSERT_GT(pid, 0);
    if (pid == 0) {
        pause();
    } else {
        // parent process
        kill(pid, SIGUSR1);
        EXPECT_TRUE(TestIsHishellLabel(pid));
    }
}

HWTEST_F(HdcMainTest, TestIsHishell_3, TestSize.Level0)
{
    pid_t pid = fork();
    ASSERT_GT(pid, 0);
    if (pid == 0) {
        pause();
    } else {
        // parent process
        kill(pid, SIGUSR1);
        EXPECT_TRUE(TestIsHishellLabel(pid));
    }
}

HWTEST_F(HdcMainTest, TestIsHishell_4, TestSize.Level0)
{
    pid_t pid = fork();
    ASSERT_GT(pid, 0);
    if (pid == 0) {
        pause();
    } else {
        // parent process
        kill(pid, SIGUSR1);
        EXPECT_TRUE(TestIsHishellLabel(pid));
    }
}
}