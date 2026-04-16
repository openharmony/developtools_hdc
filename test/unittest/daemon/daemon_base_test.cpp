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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <cerrno>
#include <unistd.h>
#include "daemon_base.h"
#include "base.h"
#include "os_account_manager.h"
#include "daemon_base_test.h"

using namespace testing;
using namespace testing::ext;

namespace {
static const int32_t DEFAULT_USER_ID = 100;
const mode_t FILE_MODE = 0755;
}

namespace Hdc {
void DaemonBaseTest::SetUpTestCase()
{
#ifdef UT_DEBUG
    Base::SetLogLevel(LOG_ALL);
#else
    Base::SetLogLevel(LOG_OFF);
#endif
}

void DaemonBaseTest::TearDownTestCase() {}

void DaemonBaseTest::SetUp() {}

void DaemonBaseTest::TearDown() {}

bool DaemonBaseTest::CreateDir(const std::string &path)
{
    if (mkdir(path.c_str(), FILE_MODE) == 0) {
        return true;
    }
    return errno == EEXIST;
}

void DaemonBaseTest::CreateTestBundlePath(const std::string &bundleName)
{
    std::string basePath = DEBUG_BUNDLE_PATH;
    std::vector<int32_t> ids;
    OHOS::ErrCode err = OHOS::AccountSA::OsAccountManager::QueryActiveOsAccountIds(ids);
    int id = -1;
    if (err != 0 || ids.empty()) {
        id = DEFAULT_USER_ID;
    } else {
        id = ids[0];
    }
    std::string userPath = basePath + std::to_string(id);
    if (!CreateDir(userPath)) {
        std::cout << "Create "<< userPath << " failed" << std::endl;
        return;
    }
    basePath += std::to_string(id) + "/debug_hap/";
    if (!CreateDir(basePath)) {
        std::cout << "Create "<< basePath << " failed" << std::endl;
        return;
    }
    std::string bundlePath = basePath + bundleName;
    if (!CreateDir(bundlePath)) {
        std::cout << "Create "<< bundlePath << " failed" << std::endl;
        return;
    }
}

void DaemonBaseTest::RemoveTestBundlePath(const std::string &bundleName)
{
    std::string bundlePath = GetTestBundlePath(bundleName);
    rmdir(bundlePath.c_str());
    
    std::string basePath = DEBUG_BUNDLE_PATH;
    std::vector<int32_t> ids;
    OHOS::ErrCode err = OHOS::AccountSA::OsAccountManager::QueryActiveOsAccountIds(ids);
    int id = -1;
    if (err != 0 || ids.empty()) {
        id = DEFAULT_USER_ID;
    } else {
        id = ids[0];
    }
    basePath += std::to_string(id) + "/debug_hap/";
    rmdir(basePath.c_str());
}

std::string DaemonBaseTest::GetTestBundlePath(const std::string &bundleName)
{
    std::string basePath = DEBUG_BUNDLE_PATH;
    std::vector<int32_t> ids;
    OHOS::ErrCode err = OHOS::AccountSA::OsAccountManager::QueryActiveOsAccountIds(ids);
    int id = -1;
    if (err != 0 || ids.empty()) {
        id = DEFAULT_USER_ID;
    } else {
        id = ids[0];
    }
    basePath += std::to_string(id) + "/debug_hap/";
    return basePath + bundleName;
}

HWTEST_F(DaemonBaseTest, TestCheckBundlePath_ValidBundle, TestSize.Level0)
{
    std::string validBundleName = "com.example.testapp";
    std::string mountPath;
    
    CreateTestBundlePath(validBundleName);
    
    bool result = HdcDaemonBase::CheckBundlePath(validBundleName, mountPath);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(mountPath.empty());
    EXPECT_NE(mountPath.find(validBundleName), std::string::npos);
    
    RemoveTestBundlePath(validBundleName);
}

HWTEST_F(DaemonBaseTest, TestCheckBundlePath_InvalidBundleName, TestSize.Level0)
{
    std::string invalidBundleName = "invalid";
    std::string mountPath;
    
    bool result = HdcDaemonBase::CheckBundlePath(invalidBundleName, mountPath);
    
    EXPECT_FALSE(result);
    EXPECT_TRUE(mountPath.empty());
}

HWTEST_F(DaemonBaseTest, TestCheckBundlePath_BundleNameTooLong, TestSize.Level0)
{
    std::string longBundleName(129, 'a');
    std::string mountPath;
    
    bool result = HdcDaemonBase::CheckBundlePath(longBundleName, mountPath);
    
    EXPECT_FALSE(result);
    EXPECT_TRUE(mountPath.empty());
}

HWTEST_F(DaemonBaseTest, TestCheckBundlePath_BundleNameTooShort, TestSize.Level0)
{
    std::string shortBundleName = "abc";
    std::string mountPath;
    
    bool result = HdcDaemonBase::CheckBundlePath(shortBundleName, mountPath);
    
    EXPECT_FALSE(result);
    EXPECT_TRUE(mountPath.empty());
}

HWTEST_F(DaemonBaseTest, TestCheckBundlePath_BundleNameWithSpecialChars, TestSize.Level0)
{
    std::string specialBundleName = "com.example#test";
    std::string mountPath;
    
    bool result = HdcDaemonBase::CheckBundlePath(specialBundleName, mountPath);
    
    EXPECT_FALSE(result);
    EXPECT_TRUE(mountPath.empty());
}

HWTEST_F(DaemonBaseTest, TestCheckBundlePath_BundleNameWithPathTraversal, TestSize.Level0)
{
    std::string pathTraversalBundle = "../../../etc/passwd";
    std::string mountPath;
    
    bool result = HdcDaemonBase::CheckBundlePath(pathTraversalBundle, mountPath);
    
    EXPECT_FALSE(result);
    EXPECT_TRUE(mountPath.empty());
}

HWTEST_F(DaemonBaseTest, TestCheckBundlePath_NonExistentBundle, TestSize.Level0)
{
    std::string nonExistentBundle = "com.example.nonexistent";
    std::string mountPath;
    
    bool result = HdcDaemonBase::CheckBundlePath(nonExistentBundle, mountPath);
    
    EXPECT_FALSE(result);
    EXPECT_TRUE(mountPath.empty());
}

HWTEST_F(DaemonBaseTest, TestCheckBundlePath_ValidBundlePathFormat, TestSize.Level0)
{
    std::string validBundleName = "com.example.validapp";
    std::string mountPath;
    
    CreateTestBundlePath(validBundleName);
    
    bool result = HdcDaemonBase::CheckBundlePath(validBundleName, mountPath);
    
    EXPECT_TRUE(result);
    
    std::string expectedPath = DEBUG_BUNDLE_PATH;
    std::vector<int32_t> ids;
    OHOS::ErrCode err = OHOS::AccountSA::OsAccountManager::QueryActiveOsAccountIds(ids);
    int id = -1;
    if (err != 0 || ids.empty()) {
        id = DEFAULT_USER_ID;
    } else {
        id = ids[0];
    }
    expectedPath += std::to_string(id) + "/debug_hap/" + validBundleName;
    EXPECT_EQ(mountPath, expectedPath);
    
    RemoveTestBundlePath(validBundleName);
}

HWTEST_F(DaemonBaseTest, TestCheckBundlePath_MultipleValidBundles, TestSize.Level0)
{
    std::vector<std::string> bundleNames = {
        "com.example.app1",
        "com.example.app2",
        "com.example.app3"
    };
    
    for (const auto &bundleName : bundleNames) {
        CreateTestBundlePath(bundleName);
    }
    
    for (const auto &bundleName : bundleNames) {
        std::string mountPath;
        bool result = HdcDaemonBase::CheckBundlePath(bundleName, mountPath);
        EXPECT_TRUE(result);
        EXPECT_FALSE(mountPath.empty());
    }
    
    for (const auto &bundleName : bundleNames) {
        RemoveTestBundlePath(bundleName);
    }
}

HWTEST_F(DaemonBaseTest, TestCheckBundlePath_EmptyBundleName, TestSize.Level0)
{
    std::string emptyBundleName = "";
    std::string mountPath;
    
    bool result = HdcDaemonBase::CheckBundlePath(emptyBundleName, mountPath);
    
    EXPECT_FALSE(result);
    EXPECT_TRUE(mountPath.empty());
}

}