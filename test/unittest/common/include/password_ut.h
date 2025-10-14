/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef HDC_PASSWORD_UT_H
#define HDC_PASSWORD_UT_H
#include "password.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace Hdc {
class HdcPasswordTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
private:
    bool CheckPasswordFormat(std::pair<uint8_t*, int> pwdInfo);
};
} // namespace Hdc
#endif // HDC_PASSWORD_UT_H