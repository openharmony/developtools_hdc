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
#ifndef HDC_CONNECT_VALID_WRAPPER_H
#define HDC_CONNECT_VALID_WRAPPER_H
#include <cstdint>

extern "C" {
int ConnValidGetParam(const char *key, char *out, int maxLen);
int ConnValidGetPubKeyHash(const char *pubKeyPath, uint8_t *hash, int maxLen);
int ConnValidGetPrivKeyInfo(const char *priKeyPath, uint8_t *info, int maxLen);
int ConnValidRsaSignBase64(const char *priKeyPath, const uint8_t *data, int dataLen,
                           char *out, int maxOut);
int ConnValidCheckPubKey(const char *pubKeyPath, const char *knownHostsDir);
}

#endif
