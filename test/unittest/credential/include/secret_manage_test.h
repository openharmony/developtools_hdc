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
#ifndef HDC_SECRET_MANAGE_TEST_H
#define HDC_SECRET_MANAGE_TEST_H
#include <gtest/gtest.h>

const std::string HDC_TEST_RAS_KEY_ALIAS = "hdc_test_ras_key_alias";

class HdcSecretManageTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();

private:
    mode_t MODE = (S_IRWXU | S_IRWXG | S_IXOTH | S_ISGID);
};

#endif  // HDC_SECRET_MANAGE_TEST_H
