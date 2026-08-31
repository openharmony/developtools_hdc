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
#ifndef HDC_SSL_WRAPPER_H
#define HDC_SSL_WRAPPER_H
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

int SslInitContext(int isServer);
void SslDestroyContext(int ctxId);
int SslDoHandshake(int ctxId, int fd);
int SslWrite(int ctxId, const uint8_t *buf, int len);
int SslRead(int ctxId, uint8_t *buf, int len);
int SslGenPsk(uint8_t *psk, int maxLen);
int SslGetPskEncrypt(const uint8_t *input, int inLen, uint8_t *output, int maxOut);
int SslRsaDecrypt(const uint8_t *input, int inLen, uint8_t *output, int maxOut,
                  const char *priKeyPath);

#ifdef __cplusplus
}
#endif

#endif
