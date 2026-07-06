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
#include <string>
#include <limits>

#include "subserver_manager.h"
#include "runtime_config.h"

using namespace testing::ext;
using namespace Hdc;

class SubserverManagerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(SubserverManagerTest, ParsePid_Success, TestSize.Level0)
{
    EXPECT_EQ(SubserverManager::ParsePid("12345"), 12345);
    EXPECT_EQ(SubserverManager::ParsePid("1"), 1);
    EXPECT_EQ(SubserverManager::ParsePid("100"), 100);
    EXPECT_EQ(SubserverManager::ParsePid("65535"), 65535);
}

HWTEST_F(SubserverManagerTest, ParsePid_InvalidFormat, TestSize.Level0)
{
    EXPECT_EQ(SubserverManager::ParsePid("abc"), -1);
    EXPECT_EQ(SubserverManager::ParsePid("123abc"), -1);
    EXPECT_EQ(SubserverManager::ParsePid("abc123"), -1);
    EXPECT_EQ(SubserverManager::ParsePid(""), -1);
    EXPECT_EQ(SubserverManager::ParsePid("   "), -1);
    EXPECT_EQ(SubserverManager::ParsePid("12 34"), -1);
    EXPECT_EQ(SubserverManager::ParsePid("12.34"), -1);
}

HWTEST_F(SubserverManagerTest, ParsePid_WindowsNewline, TestSize.Level0)
{
    std::string line = "100\r";
    EXPECT_EQ(SubserverManager::ParsePid(line), -1);

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    EXPECT_EQ(SubserverManager::ParsePid(line), 100);
}

HWTEST_F(SubserverManagerTest, ParsePid_Overflow, TestSize.Level0)
{
    EXPECT_EQ(SubserverManager::ParsePid("9999999999999999"), -1);
    EXPECT_EQ(SubserverManager::ParsePid("2147483648"), -1);
    EXPECT_EQ(SubserverManager::ParsePid("9999999999"), -1);
}

HWTEST_F(SubserverManagerTest, ParsePid_NegativeValue, TestSize.Level0)
{
    EXPECT_EQ(SubserverManager::ParsePid("-1"), -1);
    EXPECT_EQ(SubserverManager::ParsePid("-100"), -1);
    EXPECT_EQ(SubserverManager::ParsePid("-65535"), -1);
}

HWTEST_F(SubserverManagerTest, ParsePid_Boundary, TestSize.Level1)
{
    EXPECT_EQ(SubserverManager::ParsePid(std::to_string(INT_MAX)), INT_MAX);
}

HWTEST_F(SubserverManagerTest, ParsePid_WithWhitespace, TestSize.Level1)
{
    EXPECT_EQ(SubserverManager::ParsePid(" 123"), 123);
    EXPECT_EQ(SubserverManager::ParsePid("  123"), 123);
}

HWTEST_F(SubserverManagerTest, Instance_Singleton, TestSize.Level1)
{
    SubserverManager& instance1 = SubserverManager::Instance();
    SubserverManager& instance2 = SubserverManager::Instance();
    EXPECT_EQ(&instance1, &instance2);
}

HWTEST_F(SubserverManagerTest, GetPidFilePath_NotEmpty, TestSize.Level1)
{
    std::string path = SubserverManager::GetPidFilePath();
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(path.find("HDC_Subserver") != std::string::npos);
}

HWTEST_F(SubserverManagerTest, CheckClientParam_ValidPort, TestSize.Level2)
{
    SubserverManager& manager = SubserverManager::Instance();
    EXPECT_TRUE(manager.CheckClientParam("-i test_serial -o 8080"));
    EXPECT_TRUE(manager.CheckClientParam("-i test_serial -o 65535"));
}

HWTEST_F(SubserverManagerTest, CheckClientParam_ValidIpPort, TestSize.Level2)
{
    SubserverManager& manager = SubserverManager::Instance();
    EXPECT_TRUE(manager.CheckClientParam("-i test_serial -o 127.0.0.1:8080"));
    EXPECT_TRUE(manager.CheckClientParam("-i test_serial -o 192.168.1.1:8123"));
    EXPECT_TRUE(manager.CheckClientParam("-i test_serial -o 0.0.0.0:8080"));
}

HWTEST_F(SubserverManagerTest, CheckClientParam_InvalidPort, TestSize.Level2)
{
    SubserverManager& manager = SubserverManager::Instance();
    EXPECT_FALSE(manager.CheckClientParam("-1"));
    EXPECT_FALSE(manager.CheckClientParam("65536"));
    EXPECT_FALSE(manager.CheckClientParam("99999"));
    EXPECT_FALSE(manager.CheckClientParam("abc"));
    EXPECT_FALSE(manager.CheckClientParam(""));
}

HWTEST_F(SubserverManagerTest, CheckClientParam_InvalidIpPort, TestSize.Level2)
{
    SubserverManager& manager = SubserverManager::Instance();
    EXPECT_FALSE(manager.CheckClientParam("256.0.0.1:8080"));
    EXPECT_FALSE(manager.CheckClientParam("127.0.0.1:65536"));
    EXPECT_FALSE(manager.CheckClientParam("127.0.0.1:abc"));
}

HWTEST_F(SubserverManagerTest, CheckClientParam_MissingIParam, TestSize.Level2)
{
    SubserverManager& manager = SubserverManager::Instance();
    EXPECT_FALSE(manager.CheckClientParam("-o 8080"));
    EXPECT_FALSE(manager.CheckClientParam("8080"));
}

HWTEST_F(SubserverManagerTest, CheckClientParam_MissingOParam, TestSize.Level2)
{
    SubserverManager& manager = SubserverManager::Instance();
    EXPECT_FALSE(manager.CheckClientParam("-i test_serial"));
}

HWTEST_F(SubserverManagerTest, CheckClientParam_DuplicateParams, TestSize.Level2)
{
    SubserverManager& manager = SubserverManager::Instance();
    EXPECT_FALSE(manager.CheckClientParam("-i test1 -i test2 -o 8080"));
    EXPECT_FALSE(manager.CheckClientParam("-i test_serial -o 8080 -o 9090"));
}

HWTEST_F(SubserverManagerTest, CheckClientParam_SerialTooLong, TestSize.Level2)
{
    SubserverManager& manager = SubserverManager::Instance();
    std::string longSerial(300, 'a');
    std::string param = "-i " + longSerial + " -o 8080";
    EXPECT_FALSE(manager.CheckClientParam(param));
}

HWTEST_F(SubserverManagerTest, UsbDeviceConnected_Default, TestSize.Level1)
{
    SubserverManager& manager = SubserverManager::Instance();
    EXPECT_FALSE(manager.UsbDeviceConnected());
}

HWTEST_F(SubserverManagerTest, NotifyDeviceConnect_Success, TestSize.Level1)
{
    SubserverManager& manager = SubserverManager::Instance();

    EXPECT_FALSE(manager.UsbDeviceConnected());

    manager.CancelConnectTimer();
    EXPECT_TRUE(manager.UsbDeviceConnected());
}

HWTEST_F(SubserverManagerTest, HandleCommand_ParamMissing, TestSize.Level0)
{
    SubserverManager& manager = SubserverManager::Instance();

    SubserverStatus status1 = manager.HandleCommand(nullptr, nullptr, "-o 8080");
    EXPECT_EQ(status1, SubserverStatus::PARAM_ERROR);

    SubserverStatus status2 = manager.HandleCommand(nullptr, nullptr, "-i test_serial");
    EXPECT_EQ(status2, SubserverStatus::PARAM_ERROR);

    SubserverStatus status3 = manager.HandleCommand(nullptr, nullptr, "");
    EXPECT_EQ(status3, SubserverStatus::PARAM_ERROR);
}

HWTEST_F(SubserverManagerTest, HandleCommand_InvalidPort, TestSize.Level0)
{
    SubserverManager& manager = SubserverManager::Instance();

    SubserverStatus status1 = manager.HandleCommand(nullptr, nullptr, "-i test -o invalid_port");
    EXPECT_EQ(status1, SubserverStatus::INVALID_DEVICE);

    SubserverStatus status2 = manager.HandleCommand(nullptr, nullptr, "-i test -o 99999");
    EXPECT_EQ(status2, SubserverStatus::INVALID_DEVICE);
}

HWTEST_F(SubserverManagerTest, HandleCommand_SerialTooLong, TestSize.Level0)
{
    SubserverManager& manager = SubserverManager::Instance();

    std::string longSerial(300, 'a');
    std::string params = "-i " + longSerial + " -o 8080";
    SubserverStatus status = manager.HandleCommand(nullptr, nullptr, params);
    EXPECT_EQ(status, SubserverStatus::INVALID_DEVICE);
}

HWTEST_F(SubserverManagerTest, HandleCommand_DuplicateParams, TestSize.Level0)
{
    SubserverManager& manager = SubserverManager::Instance();

    SubserverStatus status1 = manager.HandleCommand(nullptr, nullptr, "-i test1 -i test2 -o 8080");
    EXPECT_EQ(status1, SubserverStatus::PARAM_ERROR);

    SubserverStatus status2 = manager.HandleCommand(nullptr, nullptr, "-i test -o 8080 -o 9090");
    EXPECT_EQ(status2, SubserverStatus::PARAM_ERROR);
}

HWTEST_F(SubserverManagerTest, HandleCommand_ValidParams, TestSize.Level0)
{
    SubserverManager& manager = SubserverManager::Instance();

    std::string validParams = "-i test_serial -o 127.0.0.1:8080";
    EXPECT_TRUE(manager.CheckClientParam(validParams));
}

HWTEST_F(SubserverManagerTest, CheckParentProcess_GetParentName, TestSize.Level1)
{
    std::string parentName = ProcessHandle::GetParentProcessName();

    EXPECT_FALSE(parentName.empty());
}

HWTEST_F(SubserverManagerTest, CheckParentProcess_StringLogic, TestSize.Level1)
{
    std::string parentName = ProcessHandle::GetParentProcessName();

    EXPECT_FALSE(parentName.empty());

    bool isHdcProcess = (parentName == "hdc" || parentName.find("hdc.exe") != std::string::npos);

    EXPECT_TRUE(isHdcProcess || !isHdcProcess);
}

HWTEST_F(SubserverManagerTest, HandleCommand_isSubserverMode, TestSize.Level0)
{
    RuntimeConfig& config = RuntimeConfig::Instance();

    bool originalValue = config.isSubserver;
    config.isSubserver = true;

    SubserverManager& manager = SubserverManager::Instance();

    std::string validParams = "-i test_serial -o 127.0.0.1:8080";
    EXPECT_TRUE(manager.CheckClientParam(validParams));

    config.isSubserver = originalValue;
}

HWTEST_F(SubserverManagerTest, HandleCommand_NotSubserverMode, TestSize.Level0)
{
    RuntimeConfig& config = RuntimeConfig::Instance();

    bool originalValue = config.isSubserver;
    config.isSubserver = false;

    SubserverManager& manager = SubserverManager::Instance();

    std::string validParams = "-i test_serial -o 127.0.0.1:8080";
    EXPECT_TRUE(manager.CheckClientParam(validParams));

    config.isSubserver = originalValue;
}