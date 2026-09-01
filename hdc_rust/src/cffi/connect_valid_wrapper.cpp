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
#include "connect_valid_wrapper.h"
#include <securec.h>
#include <cstring>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <cstdio>
#include <string>

namespace {
constexpr int SHA256_LEN = 32;
constexpr int CRED_LEN_OFFSET = 2;
constexpr int MAX_ALLOC_SIZE = 65536;

std::string Base64Encode(const uint8_t *data, int len)
{
    BIO *bio = BIO_new(BIO_s_mem());
    BIO *b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    BIO_write(bio, data, len);
    BIO_flush(bio);
    BUF_MEM *buf = nullptr;
    BIO_get_mem_ptr(bio, &buf);
    std::string result(buf->data, buf->length);
    BIO_free_all(bio);
    return result;
}
}

extern "C" {
int ConnValidGetParam(const char *key, char *out, int maxLen)
{
    if (key == nullptr || out == nullptr) {
        return -1;
    }
    const char *val = std::getenv(key);
    if (val == nullptr) {
        return -1;
    }
    int len = static_cast<int>(strlen(val));
    if (maxLen < len + 1) {
        return -1;
    }
    if (strncpy_s(out, maxLen, val, maxLen - 1) != EOK) { return -1; }
    return len;
}

int ConnValidGetPubKeyHash(const char *pubKeyPath, uint8_t *hash, int maxLen)
{
    if (pubKeyPath == nullptr || hash == nullptr) {
        return -1;
    }
    FILE *fp = fopen(pubKeyPath, "r");
    if (fp == nullptr) {
        return -1;
    }
    RSA *rsa = PEM_read_RSA_PUBKEY(fp, nullptr, nullptr, nullptr);
    if (fclose(fp) != 0) {
        return -1;
    }
    if (rsa == nullptr) {
        return -1;
    }
    uint8_t *der = nullptr;
    int derLen = i2d_RSA_PUBKEY(rsa, &der);
    RSA_free(rsa);
    if (derLen <= 0) {
        return -1;
    }
    uint8_t md[SHA256_LEN] = {0};
    SHA256(der, derLen, md);
    OPENSSL_free(der);
    int copyLen = (maxLen < SHA256_LEN) ? maxLen : SHA256_LEN;
    if (memcpy_s(hash, maxLen, md, copyLen) != EOK) { return -1; }
    return copyLen;
}

int ConnValidGetPrivKeyInfo(const char *priKeyPath, uint8_t *info, int maxLen)
{
    if (priKeyPath == nullptr || info == nullptr) {
        return -1;
    }
    FILE *fp = fopen(priKeyPath, "r");
    if (fp == nullptr) {
        return -1;
    }
    RSA *rsa = PEM_read_RSAPrivateKey(fp, nullptr, nullptr, nullptr);
    if (fclose(fp) != 0) {
        return -1;
    }
    if (rsa == nullptr) {
        return -1;
    }
    int bits = RSA_bits(rsa);
    RSA_free(rsa);
    int copyLen = sizeof(int);
    if (maxLen < copyLen) {
        return -1;
    }
    if (memcpy_s(info, maxLen, &bits, copyLen) != EOK) { return -1; }
    return copyLen;
}

int ConnValidRsaSignBase64(const char *priKeyPath, const uint8_t *data, int dataLen,
                           char *out, int maxOut)
{
    if (priKeyPath == nullptr || data == nullptr || out == nullptr) {
        return -1;
    }
    FILE *fp = fopen(priKeyPath, "r");
    if (fp == nullptr) {
        return -1;
    }
    RSA *rsa = PEM_read_RSAPrivateKey(fp, nullptr, nullptr, nullptr);
    if (fclose(fp) != 0) {
        return -1;
    }
    if (rsa == nullptr) {
        return -1;
    }
    int sigLen = RSA_size(rsa);
    if (sigLen <= 0 || sigLen > MAX_ALLOC_SIZE) {
        RSA_free(rsa);
        return -1;
    }
    uint8_t *sig = new uint8_t[sigLen];
    unsigned int sigOut = 0;
    int ret = RSA_sign(NID_sha256, data, dataLen, sig, &sigOut, rsa);
    RSA_free(rsa);
    if (ret != 1) {
        delete[] sig;
        return -1;
    }
    std::string b64 = Base64Encode(sig, sigOut);
    delete[] sig;
    if (maxOut < static_cast<int>(b64.size() + 1)) {
        return -1;
    }
    if (strncpy_s(out, maxOut, b64.c_str(), b64.size()) != EOK) { return -1; }
    return static_cast<int>(b64.size());
}

int ConnValidCheckPubKey(const char *pubKeyPath, const char *knownHostsDir)
{
    if (pubKeyPath == nullptr || knownHostsDir == nullptr) {
        return 0;
    }
    uint8_t hash[SHA256_LEN] = {0};
    int hashLen = ConnValidGetPubKeyHash(pubKeyPath, hash, SHA256_LEN);
    if (hashLen <= 0) {
        return 0;
    }
    std::string knownHosts = std::string(knownHostsDir) + "/hdc_known_hosts";
    FILE *fp = fopen(knownHosts.c_str(), "r");
    if (fp == nullptr) {
        return 0;
    }
    char line[512] = {0};
    int found = 0;
    while (fgets(line, sizeof(line), fp) != nullptr) {
        if (strstr(line, Base64Encode(hash, hashLen).c_str()) != nullptr) {
            found = 1;
            break;
        }
    }
    if (fclose(fp) != 0) {
        return 0;
    }
    return found;
}
}
