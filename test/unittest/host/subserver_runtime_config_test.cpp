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

#include "runtime_config.h"

using namespace testing::ext;
using namespace Hdc;

class RuntimeConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(RuntimeConfigTest, Instance_Singleton, TestSize.Level0)
{
    RuntimeConfig& instance1 = RuntimeConfig::Instance();
    RuntimeConfig& instance2 = RuntimeConfig::Instance();
    EXPECT_EQ(&instance1, &instance2);
}

HWTEST_F(RuntimeConfigTest, DefaultValues, TestSize.Level0)
{
    RuntimeConfig& config = RuntimeConfig::Instance();
    EXPECT_FALSE(config.isServerMode);
    EXPECT_TRUE(config.isPullServer);
    EXPECT_FALSE(config.isPcDebugRun);
    EXPECT_FALSE(config.isTCPorUSB);
    EXPECT_FALSE(config.isCustomLoglevel);
    EXPECT_FALSE(config.externalCmd);
    EXPECT_EQ(config.isTestMethod, 0);
    EXPECT_TRUE(config.connectKey.empty());
    EXPECT_TRUE(config.serverListenString.empty());
    EXPECT_TRUE(config.containerInOut.empty());
}

HWTEST_F(RuntimeConfigTest, SubserverFields, TestSize.Level0)
{
    RuntimeConfig& config = RuntimeConfig::Instance();
    EXPECT_FALSE(config.isSubserver);
    EXPECT_TRUE(config.subserverPort.empty());
    EXPECT_TRUE(config.subserverSerial.empty());
    EXPECT_TRUE(config.subserverLogFileName.empty());
}

HWTEST_F(RuntimeConfigTest, SetSubserverFields, TestSize.Level1)
{
    RuntimeConfig& config = RuntimeConfig::Instance();
    config.isSubserver = true;
    config.subserverPort = "8080";
    config.subserverSerial = "test_serial";
    config.subserverLogFileName = "test.log";

    EXPECT_TRUE(config.isSubserver);
    EXPECT_EQ(config.subserverPort, "8080");
    EXPECT_EQ(config.subserverSerial, "test_serial");
    EXPECT_EQ(config.subserverLogFileName, "test.log");

    config.isSubserver = false;
    config.subserverPort.clear();
    config.subserverSerial.clear();
    config.subserverLogFileName.clear();
}

HWTEST_F(RuntimeConfigTest, SetServerMode, TestSize.Level1)
{
    RuntimeConfig& config = RuntimeConfig::Instance();
    config.isServerMode = true;
    EXPECT_TRUE(config.isServerMode);
    config.isServerMode = false;
}

HWTEST_F(RuntimeConfigTest, SetConnectKey, TestSize.Level1)
{
    RuntimeConfig& config = RuntimeConfig::Instance();
    config.connectKey = "127.0.0.1:8080";
    EXPECT_EQ(config.connectKey, "127.0.0.1:8080");
    config.connectKey.clear();
}