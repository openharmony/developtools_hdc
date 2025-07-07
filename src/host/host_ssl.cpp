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
#ifdef HDC_SUPPORT_ENCRYPT_TCP
#include "host_ssl.h"
namespace Hdc {
HdcHostSSL::HdcHostSSL(SSLInfoPtr hSSLInfo) : HdcSSLBase(hSSLInfo)
{
}

HdcHostSSL::~HdcHostSSL()
{
    WRITE_LOG(LOG_DEBUG, "~HdcHostSSL");
}

const SSL_METHOD *HdcHostSSL::SetSSLMethod()
{
    return TLS_client_method();
}

bool HdcHostSSL::SetPskCallback()
{
    if (SSL_CTX_set_ex_data(sslCtx, 0, preSharedKey) != 1) {
        return false;
    }
    SSL_CTX_set_psk_client_callback(sslCtx, PskClientCallback);
    return true;
}

void HdcHostSSL::SetSSLState()
{
    SSL_set_connect_state(ssl);
}
} // namespace Hdc
#endif // HDC_SUPPORT_ENCRYPT_TCP