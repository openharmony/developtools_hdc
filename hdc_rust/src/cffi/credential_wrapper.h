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
#ifndef HDC_CREDENTIAL_WRAPPER_H
#define HDC_CREDENTIAL_WRAPPER_H
#include <cstdint>

constexpr uint8_t CRED_METHOD_REPORT = 1;
constexpr uint8_t CRED_METHOD_AUTH_RESULT = 2;
constexpr uint8_t CRED_METHOD_AUTH_VERIFY = 3;
constexpr uint8_t CRED_METHOD_VERSION = 4;

#pragma pack(push, 1)
struct CredentialMsg {
    uint8_t version;
    uint8_t methodType;
    uint16_t bodyLen;
    uint8_t body[0];
};
#pragma pack(pop)

struct CredentialParseOut {
    uint8_t *method;
    uint8_t *body;
    int maxBody;
    uint16_t *bodyLen;
};

extern "C" {
int CredentialBuild(uint8_t method, const uint8_t *body, uint16_t bodyLen,
                    uint8_t *out, int maxOut);
int CredentialParse(const uint8_t *data, int dataLen,
                    CredentialParseOut *out);
int CredentialSendBySocket(const char *path, const uint8_t *data, int len);
int CredentialRecvBySocket(const char *path, uint8_t *buf, int maxLen);
}

#endif
