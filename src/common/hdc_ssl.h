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

#ifndef HDC_SSL_H
#define HDC_SSL_H
#include "common.h"
namespace Hdc {
class HdcSSLBase {
public:
    explicit HdcSSLBase(const HSSLInfo &hSSLInfo);
    HdcSSLBase(const HdcSSLBase&) = delete;
    virtual ~HdcSSLBase();
    int Encrypt(const int bufLen, uint8_t *bufPtr);
    int Decrypt(const int nread, const int bufLen, uint8_t *bufPtr, int &index);
    int InitSSL();
    static int RsaPrikeyDecrypt(const unsigned char *inBuf, int inLen, unsigned char *outBuf);
    uint32_t sessionId;
    int DoBIOWrite(uint8_t *bufPtr, const int nread) const;
    int DoBIORead(uint8_t *bufPtr, const int bufLen) const;
    bool IsHandshakeFinish() const;
    int DoHandshake();
    void ShowSSLInfo();
    int GetOutPending() const; // use with BIO_read and SSL_write
    int GetInPending() const; // use with BIO_write and SSL_read
    bool ClearPsk();
    bool GenPsk();
    bool InputPsk(unsigned char *psk, int pskLen);
    int GetPskEncrypt(unsigned char *bufPtr, const int bufLen, const string &pubkey);
    static void SetSSLInfo(HSSLInfo hSSLInfo, HSession hsession);
    int PerformHandshake(vector<uint8_t> &outBuf);
    bool SetHandshakeLabel(HSession hSession);
    inline static int GetSSLBufLen(const int bufLen)
    {
        return (bufLen + (((bufLen - 1) / BUF_SIZE_DEFAULT16) + 1) * BUF_SIZE_SSL_HEAD);
    }

private:
    static int RsaPubkeyEncrypt(const uint32_t sessionId,
        const unsigned char *inBuf, int inLen, unsigned char *outBuf, const string &pubkey);
    int DoSSLWrite(const int bufLen, uint8_t *bufPtr);
    int DoSSLRead(const int bufLen, int &index, uint8_t *bufPtr);
    bool isDaemon;
    bool isInited = false;

protected:
    static unsigned int PskServerCallback(SSL *ssl, const char *identity,
        unsigned char *psk, unsigned int maxPskLen);
    static unsigned int PskClientCallback(SSL *ssl, const char *hint,
        char *identity, unsigned int maxIdentityLen, unsigned char *psk, unsigned int maxPskLen);
    virtual bool SetPskCallback() = 0;
    virtual void SetSSLState() = 0;
    virtual const SSL_METHOD *SetSSLMethod() = 0;
    unsigned char preSharedKey[32]; // pre-shared key for TLS 1.3, TLS_AES_128_GCM_SHA256(password)
    string cipher;
    SSL_CTX *sslCtx;
    SSL *ssl;
    BIO *inBIO; // SSL decrypt: BIO_write from buffer to "in" , then SSL read from "in" to another buffer.
    BIO *outBIO; // SSL encrypt: SSL_write form buffer, then BIO_read from "out" to another buffer.
};
} // namespace Hdc
#endif // HDC_SSL_H