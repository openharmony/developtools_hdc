/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <vector>

#include "file_lock_util.h"

using namespace testing::ext;
using namespace Hdc;

class FileLockUtilTest : public ::testing::Test {
protected:
    std::string testFilePath;

    void SetUp() override
    {
        testFilePath = "/data/local/tmp/hdc_test_lock_" + std::to_string(getpid()) + ".txt";
    }

    void TearDown() override
    {
        unlink(testFilePath.c_str());
    }
};

HWTEST_F(FileLockUtilTest, AtomicAppend_Success, TestSize.Level0)
{
    std::string content = "test line 1\n";
    EXPECT_TRUE(FileLockUtil::AtomicAppend(testFilePath, content));

    std::string readContent;
    EXPECT_TRUE(FileLockUtil::AtomicReadAndClear(testFilePath, readContent));
    EXPECT_EQ(readContent, content);
}

HWTEST_F(FileLockUtilTest, AtomicAppend_MultipleLines, TestSize.Level0)
{
    std::string line1 = "line1\n";
    std::string line2 = "line2\n";
    std::string line3 = "line3\n";

    EXPECT_TRUE(FileLockUtil::AtomicAppend(testFilePath, line1));
    EXPECT_TRUE(FileLockUtil::AtomicAppend(testFilePath, line2));
    EXPECT_TRUE(FileLockUtil::AtomicAppend(testFilePath, line3));

    std::string readContent;
    EXPECT_TRUE(FileLockUtil::AtomicReadAndClear(testFilePath, readContent));
    EXPECT_EQ(readContent, line1 + line2 + line3);
}

HWTEST_F(FileLockUtilTest, AtomicRemoveLine_Success, TestSize.Level0)
{
    FileLockUtil::AtomicAppend(testFilePath, "line1\n");
    FileLockUtil::AtomicAppend(testFilePath, "line2\n");
    FileLockUtil::AtomicAppend(testFilePath, "line3\n");

    EXPECT_TRUE(FileLockUtil::AtomicRemoveLine(testFilePath, "line2"));

    std::string readContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, readContent);
    EXPECT_EQ(readContent, "line1\nline3\n");
}

HWTEST_F(FileLockUtilTest, AtomicRemoveLine_FirstLine, TestSize.Level0)
{
    FileLockUtil::AtomicAppend(testFilePath, "line1\n");
    FileLockUtil::AtomicAppend(testFilePath, "line2\n");

    EXPECT_TRUE(FileLockUtil::AtomicRemoveLine(testFilePath, "line1"));

    std::string readContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, readContent);
    EXPECT_EQ(readContent, "line2\n");
}

HWTEST_F(FileLockUtilTest, AtomicRemoveLine_LastLine, TestSize.Level0)
{
    FileLockUtil::AtomicAppend(testFilePath, "line1\n");
    FileLockUtil::AtomicAppend(testFilePath, "line2\n");

    EXPECT_TRUE(FileLockUtil::AtomicRemoveLine(testFilePath, "line2"));

    std::string readContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, readContent);
    EXPECT_EQ(readContent, "line1\n");
}

HWTEST_F(FileLockUtilTest, AtomicReadAndClear_Success, TestSize.Level0)
{
    FileLockUtil::AtomicAppend(testFilePath, "content\n");

    std::string readContent;
    EXPECT_TRUE(FileLockUtil::AtomicReadAndClear(testFilePath, readContent));
    EXPECT_EQ(readContent, "content\n");

    std::string emptyContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, emptyContent);
    EXPECT_TRUE(emptyContent.empty());
}

HWTEST_F(FileLockUtilTest, AtomicReadAndClear_EmptyFile, TestSize.Level0)
{
    std::string readContent;
    EXPECT_TRUE(FileLockUtil::AtomicReadAndClear(testFilePath, readContent));
    EXPECT_TRUE(readContent.empty());
}

HWTEST_F(FileLockUtilTest, WithLock_CallbackSuccess, TestSize.Level0)
{
    bool result = FileLockGuard::WithLock(testFilePath, [](FileLockGuard& guard) {
        return guard.Rewrite("test content");
    });
    EXPECT_TRUE(result);

    std::string readContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, readContent);
    EXPECT_EQ(readContent, "test content");
}

HWTEST_F(FileLockUtilTest, WithLock_CallbackFail, TestSize.Level0)
{
    bool result = FileLockGuard::WithLock(testFilePath, [](FileLockGuard& guard) {
        return false;
    });
    EXPECT_FALSE(result);

    bool retry = FileLockGuard::WithLock(testFilePath, [](FileLockGuard& guard) {
        return true;
    });
    EXPECT_TRUE(retry);
}

HWTEST_F(FileLockUtilTest, AtomicAppend_EmptyContent, TestSize.Level1)
{
    EXPECT_TRUE(FileLockUtil::AtomicAppend(testFilePath, ""));

    std::string readContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, readContent);
    EXPECT_TRUE(readContent.empty());
}

HWTEST_F(FileLockUtilTest, AtomicRemoveLine_NotFound, TestSize.Level1)
{
    FileLockUtil::AtomicAppend(testFilePath, "line1\n");
    EXPECT_TRUE(FileLockUtil::AtomicRemoveLine(testFilePath, "not_exist_line"));

    std::string readContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, readContent);
    EXPECT_EQ(readContent, "line1\n");
}

HWTEST_F(FileLockUtilTest, AtomicRemoveLine_EmptyFile, TestSize.Level1)
{
    EXPECT_TRUE(FileLockUtil::AtomicRemoveLine(testFilePath, "any_line"));

    std::string readContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, readContent);
    EXPECT_TRUE(readContent.empty());
}

HWTEST_F(FileLockUtilTest, Rewrite_Success, TestSize.Level1)
{
    bool result = FileLockGuard::WithLock(testFilePath, [](FileLockGuard& guard) {
        guard.Rewrite("old content");
        return guard.Rewrite("new content");
    });
    EXPECT_TRUE(result);

    std::string readContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, readContent);
    EXPECT_EQ(readContent, "new content");
}

HWTEST_F(FileLockUtilTest, Truncate_Success, TestSize.Level1)
{
    FileLockUtil::AtomicAppend(testFilePath, "content\n");

    bool result = FileLockGuard::WithLock(testFilePath, [](FileLockGuard& guard) {
        return guard.Truncate();
    });
    EXPECT_TRUE(result);

    std::string readContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, readContent);
    EXPECT_TRUE(readContent.empty());
}

HWTEST_F(FileLockUtilTest, FilterLine_Success, TestSize.Level2)
{
    FileLockUtil::AtomicAppend(testFilePath, "line1\n");
    FileLockUtil::AtomicAppend(testFilePath, "line2\n");
    FileLockUtil::AtomicAppend(testFilePath, "line3\n");

    bool result = FileLockGuard::WithLock(testFilePath, [](FileLockGuard& guard) {
        return guard.FilterLine("line2");
    });
    EXPECT_TRUE(result);

    std::string readContent;
    FileLockUtil::AtomicReadAndClear(testFilePath, readContent);
    EXPECT_EQ(readContent, "line1\nline3\n");
}