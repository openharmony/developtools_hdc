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
#include <cstring>
#include <string>
#include <unistd.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base.h"
#include "define.h"

using namespace testing::ext;
using namespace testing;

namespace Hdc {
class UpdateCmdLogSwitchTest : public ::testing::Test {
    private:
        static void SetUpTestCase(void);
        static void TearDownTestCase(void);
        void SetUp();
        void TearDown();
};

void UpdateCmdLogSwitchTest::SetUpTestCase() {}
void UpdateCmdLogSwitchTest::TearDownTestCase() {}
void UpdateCmdLogSwitchTest::SetUp() {}
void UpdateCmdLogSwitchTest::TearDown() {}
    
HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_12, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "12", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_1, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "1", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_TRUE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_0, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "0", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_a, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "a", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

} // namespace Hdc