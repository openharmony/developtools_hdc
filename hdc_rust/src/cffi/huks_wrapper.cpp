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
#include "huks_wrapper.h"
#include <securec.h>
#include <cstring>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

namespace {
constexpr int AES_KEY_LEN = 32;
constexpr int AES_IV_LEN = 16;
constexpr int RSA_BITS = 3072;
uint8_t g_aesKey[AES_KEY_LEN] = {0};
uint8_t g_aesIv[AES_IV_LEN] = {0};
bool g_initialized = false;

void EnsureInit()
{
    if (!g_initialized) {
        RAND_bytes(g_aesKey, AES_KEY_LEN);
        RAND_bytes(g_aesIv, AES_IV_LEN);
        g_initialized = true;
    }
}
}

extern "C" {
int HuksAesGcmEncrypt(const uint8_t *plain, int plainLen, uint8_t *cipher, int maxOut)
{
    if (plain == nullptr || cipher == nullptr) {
        return -1;
    }
    EnsureInit();
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        return -1;
    }
    int outLen = 0;
    int totalLen = 0;
    int ret = -1;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, g_aesKey, g_aesIv) == 1) {
        if (EVP_EncryptUpdate(ctx, cipher, &outLen, plain, plainLen) == 1) {
            totalLen = outLen;
            int finLen = 0;
            if (EVP_EncryptFinal_ex(ctx, cipher + outLen, &finLen) == 1) {
                totalLen += finLen;
                ret = totalLen;
            }
        }
    }
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}

int HuksAesGcmDecrypt(const uint8_t *cipher, int cipherLen, uint8_t *plain, int maxOut)
{
    if (cipher == nullptr || plain == nullptr) {
        return -1;
    }
    EnsureInit();
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        return -1;
    }
    int outLen = 0;
    int totalLen = 0;
    int ret = -1;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, g_aesKey, g_aesIv) == 1) {
        if (EVP_DecryptUpdate(ctx, plain, &outLen, cipher, cipherLen) == 1) {
            totalLen = outLen;
            int finLen = 0;
            if (EVP_DecryptFinal_ex(ctx, plain + outLen, &finLen) == 1) {
                totalLen += finLen;
                ret = totalLen;
            }
        }
    }
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}

int HuksGenRsaPublicKey(uint8_t *pubKey, int maxLen)
{
    if (pubKey == nullptr) {
        return -1;
    }
    RSA *rsa = RSA_new();
    BIGNUM *bn = BN_new();
    BN_set_word(bn, RSA_F4);
    if (RSA_generate_key_ex(rsa, RSA_BITS, bn, nullptr) != 1) {
        BN_free(bn);
        RSA_free(rsa);
        return -1;
    }
    BN_free(bn);
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_RSA_PUBKEY(bio, rsa);
    int len = BIO_read(bio, pubKey, maxLen);
    BIO_free(bio);
    RSA_free(rsa);
    return len;
}

int HuksRsaDecryptPrivate(const uint8_t *input, int inLen, uint8_t *output, int maxOut)
{
    if (input == nullptr || output == nullptr) {
        return -1;
    }
    EnsureInit();
    RSA *rsa = RSA_new();
    BIGNUM *bn = BN_new();
    BN_set_word(bn, RSA_F4);
    if (RSA_generate_key_ex(rsa, RSA_BITS, bn, nullptr) != 1) {
        BN_free(bn);
        RSA_free(rsa);
        return -1;
    }
    BN_free(bn);
    int ret = RSA_private_decrypt(inLen, input, output, rsa, RSA_PKCS1_PADDING);
    RSA_free(rsa);
    return ret;
}

void HuksResetKey()
{
    g_initialized = false;
    if (memset_s(g_aesKey, sizeof(g_aesKey), 0, AES_KEY_LEN) != EOK) { return; }
    if (memset_s(g_aesIv, sizeof(g_aesIv), 0, AES_IV_LEN) != EOK) { return; }
}
}
