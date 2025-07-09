/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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
#ifndef HDC_HOST_SSL_H
#define HDC_HOST_SSL_H
#include "hdc_ssl.h"

namespace Hdc {
class HdcHostSSL : public HdcSSLBase {
public:
    explicit HdcHostSSL(SSLInfoPtr hSSLInfo);
    ~HdcHostSSL() override;
    const SSL_METHOD *SetSSLMethod() override;
    bool SetPskCallback() override;
    void SetSSLState() override;
};
} // namespace Hdc
#endif // HDC_HOST_SSL_H