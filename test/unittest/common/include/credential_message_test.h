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
#ifndef HDC_CREDENTIAL_MESSAGE_TEST_H
#define HDC_CREDENTIAL_MESSAGE_TEST_H
#include <gtest/gtest.h>
namespace Hdc {
class HdcCredentialMessageTest : public ::testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
    bool IsNumeric(const std::string& str);
    int StripLeadingZeros(const std::string& input);
    std::vector<uint8_t> String2Uint8(const std::string& str, size_t len);
    std::string IntToStringWithPadding(int length, int maxLen);
};
}   // namespace Hdc
#endif  // HDC_CREDENTIAL_MESSAGE_TEST_H