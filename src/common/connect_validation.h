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

#ifndef HDC_CONNECT_VALIDATION_H
#define HDC_CONNECT_VALIDATION_H

#include "auth.h"
#include "base.h"
#include "common.h"
#include "credential_message.h"
#include "log.h"

namespace HdcValidation {
int GetConnectValidationParam();
#ifdef HDC_HOST
bool GetPublicKeyHashInfo(string &pubkey_info);
bool GetPrivateKeyInfo(string &privkey_info);
bool RsaSignAndBase64(string &buf, Hdc::AuthVerifyType type, string &pemStr);
#else
bool CheckAuthPubKeyIsValid(string &pubkey);
#endif
}  // namespace HdcValidation
#endif  // HDC_CONNECT_VALIDATION_H
