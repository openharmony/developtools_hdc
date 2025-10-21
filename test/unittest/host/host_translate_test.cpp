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
#include "host_translate_test.h"
#include "translate.h"

using namespace testing::ext;

namespace Hdc {
void HdcTranslateTest::SetUpTestCase() {}
void HdcTranslateTest::TearDownTestCase() {}
void HdcTranslateTest::SetUp()
{
#ifdef UT_DEBUG
    Base::SetLogLevel(LOG_ALL);
#else
    Base::SetLogLevel(LOG_OFF);
#endif
}
void HdcTranslateTest::TearDown() {}

HWTEST_F(HdcTranslateTest, TestHdcHelp, TestSize.Level0)
{
    std::string usage = TranslateCommand::Usage();
    EXPECT_FALSE(usage.empty());
    EXPECT_NE(usage.find("-l[0-5]"), string::npos);
    EXPECT_NE(usage.find("checkserver"), string::npos);
    EXPECT_NE(usage.find("tconn key [-remove]"), string::npos);
    EXPECT_NE(usage.find("-s [ip:]port"), string::npos);
    EXPECT_NE(usage.find("-cwd: specify the working directory"), string::npos);
}

HWTEST_F(HdcTranslateTest, TestHdcHelp_OHOS, TestSize.Level0)
{
#ifdef HOST_OHOS
    std::string usage = TranslateCommand::Usage();
    EXPECT_FALSE(usage.empty());
    EXPECT_NE(usage.find("-s [ip:]port | uds"), string::npos);
    EXPECT_EQ(usage.find("keygen FILE"), string::npos);
#endif
}

HWTEST_F(HdcTranslateTest, TestHdcHelpVerbose, TestSize.Level0)
{
    std::string usage = TranslateCommand::Verbose();
    EXPECT_FALSE(usage.empty());
    EXPECT_NE(usage.find("-l[0-5]"), string::npos);
    EXPECT_NE(usage.find("checkserver"), string::npos);
    EXPECT_NE(usage.find("tconn key [-remove]"), string::npos);
    EXPECT_NE(usage.find("-s [ip:]port"), string::npos);
    EXPECT_NE(usage.find("flash commands"), string::npos);
}

HWTEST_F(HdcTranslateTest, TestHdcHelpVerbose_OHOS, TestSize.Level0)
{
#ifdef HOST_OHOS
    std::string usage = TranslateCommand::Verbose();
    EXPECT_FALSE(usage.empty());
    EXPECT_NE(usage.find("-s [ip:]port | uds"), string::npos);
    EXPECT_EQ(usage.find("keygen FILE"), string::npos);
#endif
}
}