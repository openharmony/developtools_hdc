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
#include "password_wrapper.h"
#include <securec.h>
#include "huks_wrapper.h"
#include <securec.h>
#include <cstring>
#include <openssl/rand.h>

namespace {
constexpr int PWD_LEN = 16;
uint8_t g_password[PWD_LEN] = {0};
uint8_t g_encrypted[PWD_LEN * 4] = {0};
int g_encLen = 0;
}

extern "C" {
int PwdGenerate(uint8_t *pwd, int maxLen)
{
    if (pwd == nullptr || maxLen < PWD_LEN) {
        return -1;
    }
    if (RAND_bytes(pwd, PWD_LEN) != 1) {
        return -1;
    }
    if (memcpy_s(g_password, sizeof(g_password), pwd, PWD_LEN) != EOK) { return -1; }
    g_encLen = HuksAesGcmEncrypt(g_password, PWD_LEN, g_encrypted, sizeof(g_encrypted));
    return g_encLen > 0 ? PWD_LEN : -1;
}

int PwdEncrypt(const uint8_t *plain, int plainLen, uint8_t *cipher, int maxOut)
{
    if (plain == nullptr || cipher == nullptr) {
        return -1;
    }
    return HuksAesGcmEncrypt(plain, plainLen, cipher, maxOut);
}

int PwdDecrypt(const uint8_t *cipher, int cipherLen, uint8_t *plain, int maxOut)
{
    if (cipher == nullptr || plain == nullptr) {
        return -1;
    }
    return HuksAesGcmDecrypt(cipher, cipherLen, plain, maxOut);
}

void PwdReset()
{
    if (memset_s(g_password, sizeof(g_password), 0, PWD_LEN) != EOK) { return; }
    if (memset_s(g_encrypted, sizeof(g_encrypted), 0, sizeof(g_encrypted)) != EOK) { return; }
    g_encLen = 0;
    HuksResetKey();
}
}
