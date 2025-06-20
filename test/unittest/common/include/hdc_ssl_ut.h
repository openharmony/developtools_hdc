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
#ifndef HDC_SSL_UT_H
#define HDC_SSL_UT_H
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "hdc_ssl.h"
#include "host_ssl.h"
#include "daemon_ssl.h"

namespace Hdc {
class HdcSSLTest : public testing::Test {
protected:
    HdcSSLTest();
    ~HdcSSLTest() override;
    void SetUp() override;
    void TearDown() override;
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
};

} // namespace Hdc
#endif // HDC_SSL_UT_H