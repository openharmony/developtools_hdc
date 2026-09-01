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
#include "ssl_wrapper.h"
#include <cstring>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <map>
#include <mutex>

namespace {
constexpr int PSK_LEN = 32;
constexpr int RSA_KEY_BITS = 3072;
std::mutex g_mutex;
int g_nextId = 1;
std::map<int, SSL_CTX *> g_ctxMap;
std::map<int, SSL *> g_sslMap;

SSL_CTX *GetCtx(int ctxId)
{
    auto it = g_ctxMap.find(ctxId);
    return it != g_ctxMap.end() ? it->second : nullptr;
}

SSL *GetSsl(int ctxId)
{
    auto it = g_sslMap.find(ctxId);
    return it != g_sslMap.end() ? it->second : nullptr;
}
}

extern "C" {
int SslInitContext(int isServer)
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    const SSL_METHOD *method = isServer ? TLS_server_method() : TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (ctx == nullptr) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    int id = g_nextId++;
    g_ctxMap[id] = ctx;
    return id;
}

void SslDestroyContext(int ctxId)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_ctxMap.find(ctxId);
    if (it != g_ctxMap.end()) {
        auto sslIt = g_sslMap.find(ctxId);
        if (sslIt != g_sslMap.end()) {
            SSL_free(sslIt->second);
            g_sslMap.erase(sslIt);
        }
        SSL_CTX_free(it->second);
        g_ctxMap.erase(it);
    }
}

int SslDoHandshake(int ctxId, int fd)
{
    SSL_CTX *ctx = GetCtx(ctxId);
    if (ctx == nullptr) {
        return -1;
    }
    SSL *ssl = SSL_new(ctx);
    if (ssl == nullptr) {
        return -1;
    }
    SSL_set_fd(ssl, fd);
    int ret = SSL_accept(ssl);
    if (ret <= 0) {
        SSL_free(ssl);
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_sslMap[ctxId] = ssl;
    return 1;
}

int SslWrite(int ctxId, const uint8_t *buf, int len)
{
    SSL *ssl = GetSsl(ctxId);
    if (ssl == nullptr || buf == nullptr) {
        return -1;
    }
    return SSL_write(ssl, buf, len);
}

int SslRead(int ctxId, uint8_t *buf, int len)
{
    SSL *ssl = GetSsl(ctxId);
    if (ssl == nullptr || buf == nullptr) {
        return -1;
    }
    return SSL_read(ssl, buf, len);
}

int SslGenPsk(uint8_t *psk, int maxLen)
{
    if (psk == nullptr || maxLen < PSK_LEN) {
        return -1;
    }
    return RAND_bytes(psk, PSK_LEN) == 1 ? PSK_LEN : -1;
}

int SslGetPskEncrypt(const uint8_t *input, int inLen, uint8_t *output, int maxOut)
{
    if (input == nullptr || output == nullptr) {
        return -1;
    }
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        return -1;
    }
    uint8_t key[PSK_LEN] = {0};
    uint8_t iv[16] = {0};
    int outLen = 0;
    int ret = -1;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, key, iv) == 1) {
        if (EVP_EncryptUpdate(ctx, output, &outLen, input, inLen) == 1) {
            int finLen = 0;
            if (EVP_EncryptFinal_ex(ctx, output + outLen, &finLen) == 1) {
                ret = outLen + finLen;
            }
        }
    }
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}

int SslRsaDecrypt(const uint8_t *input, int inLen, uint8_t *output, int maxOut,
                  const char *priKeyPath)
{
    if (input == nullptr || output == nullptr || priKeyPath == nullptr) {
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
    int ret = RSA_private_decrypt(inLen, input, output, rsa, RSA_PKCS1_PADDING);
    RSA_free(rsa);
    return ret;
}
}
