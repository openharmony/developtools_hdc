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
#ifndef HDC_COMMAND_EVENT_REPORT_TEST_H
#define HDC_COMMAND_EVENT_REPORT_TEST_H
#include <gtest/gtest.h>

#include "common.h"
namespace Hdc {
class HdcCommandEventReportTest : public ::testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
    std::string GetCallerName(Base::Caller caller);
    bool GetCommandFromInputRaw(const char* inputRaw, std::string &command);
};
}   // namespace Hdc
#endif  // HDC_COMMAND_EVENT_REPORT_TEST_H