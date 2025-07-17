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
#include "hdc_ssl.h"

namespace Hdc {
HdcSSLBase::HdcSSLBase(SSLInfoPtr hSSLInfo)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100003L
    if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, NULL) == 0) {
        WRITE_LOG(LOG_FATAL, "OPENSSL_init_ssl");
    }
    ERR_clear_error();
#else
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
#endif
    cipher = hSSLInfo->cipher;
    sessionId = hSSLInfo->sessionId;
    isDaemon = hSSLInfo->isDaemon;
    if (memset_s(preSharedKey, sizeof(preSharedKey), 0, sizeof(preSharedKey)) != EOK) {
        WRITE_LOG(LOG_FATAL, "memset_s preSharedKey failed");
    }
}

HdcSSLBase::~HdcSSLBase()
{
    if (!isInited) {
        return;
    }
    BIO_reset(outBIO);
    BIO_reset(inBIO);
    SSL_free(ssl);
    inBIO = nullptr;
    outBIO = nullptr;
    ssl = nullptr;
    SSL_CTX_free(sslCtx);
    sslCtx = nullptr;
    WRITE_LOG(LOG_INFO, "SSL free finished for sid:%u", sessionId);
    isInited = false;
}

void HdcSSLBase::SetSSLInfo(SSLInfoPtr hSSLInfo, HSession hSession)
{
    hSSLInfo->cipher = TLS_AES_128_GCM_SHA256;
    hSSLInfo->isDaemon = !hSession->serverOrDaemon;
    hSSLInfo->sessionId = hSession->sessionId;
}

int HdcSSLBase::InitSSL()
{
    const SSL_METHOD *method = SetSSLMethod();
    sslCtx = SSL_CTX_new(method);
    if (sslCtx == nullptr) {
        WRITE_LOG(LOG_FATAL, "SSL_CTX_new failed");
        return ERR_GENERIC;
    }
    SetPskCallback();
    SSL_CTX_set_ciphersuites(sslCtx, cipher.c_str());
    inBIO = BIO_new(BIO_s_mem());
    outBIO = BIO_new(BIO_s_mem());
    if (inBIO == nullptr || outBIO == nullptr) {
        WRITE_LOG(LOG_FATAL, "BIO_new failed");
        return ERR_GENERIC;
    }
    ssl = SSL_new(sslCtx);
    if (ssl == nullptr) {
        WRITE_LOG(LOG_FATAL, "SSL_new failed");
        return ERR_GENERIC;
    }
    SetSSLState();
    SSL_set_bio(ssl, inBIO, outBIO);
    isInited = true;
    WRITE_LOG(LOG_DEBUG, "SSL init finished for sid:%u", sessionId);
    return RET_SUCCESS;
}

int HdcSSLBase::DoSSLWrite(const int bufLen, uint8_t *bufPtr)
{
    int retSSL = SSL_write(ssl, bufPtr, bufLen);
    if (retSSL < 0) {
        WRITE_LOG(LOG_WARN, "SSL write error, ret:%d", retSSL);
        int err = SSL_get_error(ssl, retSSL);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            DEBUG_LOG("SSL write error, ret:%d, err:%d, retry");
        } else {
            WRITE_LOG(LOG_WARN, "SSL write error, ret:%d, err:%d", retSSL, err);
        }
    }
    return retSSL;
}

int HdcSSLBase::Encrypt(const int bufLen, uint8_t *bufPtr)
{
    int retSSL = DoSSLWrite(bufLen, bufPtr);
    if (retSSL < 0) {
        return retSSL;
    }
    int sslBufLen = GetOutPending();
    int retBIO = DoBIORead(bufPtr, sslBufLen);
    return retBIO;
}

int HdcSSLBase::DoSSLRead(const int bufLen, int &index, uint8_t *bufPtr)
{
    int nSSLRead = SSL_read(ssl, bufPtr + index, std::min(static_cast<int>(BUF_SIZE_DEFAULT16), bufLen - index));
    if (nSSLRead < 0) {
        int err = SSL_get_error(ssl, nSSLRead);
        if (err == SSL_ERROR_WANT_READ) {
            DEBUG_LOG("SSL_ERROR_WANT_READ, availTailIndex: %d", index);
            if (index > static_cast<int>(BUF_SIZE_DEFAULT16)) {
                return RET_SUCCESS;
            }
            return ERR_DECRYPT_WANT_READ;
        }
        WRITE_LOG(LOG_FATAL, "nSSLRead is failed errno: %d", err);
        return ERR_GENERIC;
    } else {
        index += nSSLRead;
        return GetInPending();
    }
}

int HdcSSLBase::DoBIOWrite(uint8_t *bufPtr, const int nread) const
{
    return BIO_write(inBIO, bufPtr, nread);
}

int HdcSSLBase::DoBIORead(uint8_t *bufPtr, const int bufLen) const
{
    return BIO_read(outBIO, bufPtr, bufLen);
}


bool HdcSSLBase::IsHandshakeFinish() const
{
    // return 0 if handshake is finished, else return 1
    return (SSL_is_init_finished(ssl) != 0);
}

int HdcSSLBase::DoHandshake()
{
    int ret = SSL_do_handshake(ssl);
    if (ret < 0) {
        int err = SSL_get_error(ssl, ret);
        if (err != SSL_ERROR_WANT_READ) {
            WRITE_LOG(LOG_DEBUG, "SSL_do_handshake error ret is %d ", err);
        }
    }
    return ret;
}

// use with BIO_read and SSL_write
int HdcSSLBase::GetOutPending() const
{
    return BIO_pending(outBIO);
}

// use with BIO_write and SSL_read
int HdcSSLBase::GetInPending() const
{
    return BIO_pending(inBIO);
}

void HdcSSLBase::ShowSSLInfo()
{
    WRITE_LOG(LOG_INFO, "SSL handshake status is %d, version is %s, cipher is %s",
        SSL_is_init_finished(ssl), SSL_get_version(ssl), SSL_get_cipher_name(ssl));
}

bool HdcSSLBase::ClearPsk()
{
    // NOTE: 32 is the max length of psk
    if (memset_s(preSharedKey, sizeof(preSharedKey), 0, sizeof(preSharedKey)) != EOK) {
        WRITE_LOG(LOG_FATAL, "ClearPsk memset_s failed");
        return false;
    }
    return true;
}

bool HdcSSLBase::InputPsk(unsigned char *psk, int pskLen)
{
    if (pskLen > BUF_SIZE_PSK) {
        WRITE_LOG(LOG_FATAL, "pskLen is too long, pskLen = %d", pskLen);
        return false;
    }
    if (memcpy_s(preSharedKey, sizeof(preSharedKey), psk, pskLen) != EOK) {
        WRITE_LOG(LOG_FATAL, "memcpy_s failed");
        return false;
    }
    return true;
}

bool HdcSSLBase::GenPsk()
{
    unsigned char* buf = preSharedKey;
    if (RAND_priv_bytes(buf, BUF_SIZE_PSK) != 1) {
        WRITE_LOG(LOG_FATAL, "RAND_priv_bytes failed");
        return false;
    }
    return true;
}

int HdcSSLBase::GetPskEncrypt(unsigned char *bufPtr, const int bufLen, const string &pubkey)
{
    if (bufLen < BUF_SIZE_PSK_ENCRYPTED) {
        WRITE_LOG(LOG_FATAL, "bufLen is too short, bufLen = %d", bufLen);
        return ERR_GENERIC;
    }
    unsigned char* buf = preSharedKey;
    int payloadSize = RsaPubkeyEncrypt(buf, BUF_SIZE_PSK, bufPtr, bufLen, pubkey);
    WRITE_LOG(LOG_INFO, "RsaPubkeyEncrypt payloadSize = %d, sid: %u", payloadSize, sessionId);
    return payloadSize; // return the size of encrypted psk
}

int HdcSSLBase::Decrypt(const int nread, const int bufLen, uint8_t *bufPtr, int &index)
{
    // the bufPtr need at least BUF_SIZE_DEFAULT16
    if (!SSL_is_init_finished(ssl)) {
        WRITE_LOG(LOG_WARN, "SSL is not init finished");
        return ERR_GENERIC;
    }
    int left = nread;
    int retBio = DoBIOWrite(bufPtr + index, nread); // write to "in"
    if (retBio < 0) {
        WRITE_LOG(LOG_WARN, "BIO write failed, ret is %d", retBio);
        return ERR_GENERIC;
    }
    while (left > 0) {
        int result = DoSSLRead(bufLen, index, bufPtr); // read from ssl, output to bufPtr
        if (result < 0) {
            return result;
        }
        left = result;
        DEBUG_LOG("nread=%d, retbio=%d, sslread = %d, left = %d", nread, retBio, index, left);
    }
    return RET_SUCCESS;
}

unsigned int HdcSSLBase::PskServerCallback(SSL *ssl, const char *identity, unsigned char *psk, unsigned int maxPskLen)
{
    SSL_CTX *sslctx = SSL_get_SSL_CTX(ssl);
    void *exData = SSL_CTX_get_ex_data(sslctx, 0);
    if (exData == nullptr) {
        WRITE_LOG(LOG_FATAL, "exData is null");
        return 0;
    }
    unsigned char *pskInput = reinterpret_cast<unsigned char*>(exData);
    if (strcmp(identity, STR_PSK_IDENTITY.c_str()) != 0) {
        WRITE_LOG(LOG_FATAL, "identity not same");
        return 0;
    }
    unsigned int keyLen = BUF_SIZE_PSK;
    if (keyLen > maxPskLen) {
        WRITE_LOG(LOG_FATAL, "Server PSK key length invalid, maxpsklen = %d, keyLen = %d", maxPskLen, keyLen);
        return 0;
    }
    if (memcpy_s(psk, maxPskLen, pskInput, keyLen) != EOK) {
        WRITE_LOG(LOG_FATAL, "memcpy failed, maxpsklen = %d, keyLen = %d", maxPskLen, keyLen);
        return 0;
    }
    return keyLen;
}

unsigned int HdcSSLBase::PskClientCallback(SSL *ssl, const char *hint, char *identity, unsigned int maxIdentityLen,
    unsigned char *psk, unsigned int maxPskLen)
{
    SSL_CTX *sslctx = SSL_get_SSL_CTX(ssl);
    void *exData = SSL_CTX_get_ex_data(sslctx, 0);
    if (exData == nullptr) {
        WRITE_LOG(LOG_FATAL, "exData is null");
        return 0;
    }
    unsigned char *pskInput = reinterpret_cast<unsigned char*>(exData);
    if (STR_PSK_IDENTITY.size() + 1 > maxIdentityLen) {
        WRITE_LOG(LOG_FATAL, "Client identity buffer too small, maxIdentityLen = %d", maxIdentityLen);
        return 0;
    }
    if (strcpy_s(identity, maxIdentityLen, "Client_identity") != EOK) {
        WRITE_LOG(LOG_FATAL, "Client PSK key strcpy_s identity failed, maxIdentityLen is %u", maxIdentityLen);
        return 0;
    }
    unsigned int keyLen = BUF_SIZE_PSK;
    if (keyLen > maxPskLen) {
        WRITE_LOG(LOG_FATAL, "Client PSK key length invalid, maxpsklen = %d, keyLen = %d", maxPskLen, keyLen);
        return 0;
    }
    if (memcpy_s(psk, maxPskLen, pskInput, keyLen) != EOK) {
        WRITE_LOG(LOG_INFO, "memcpy failed, maxpsklen = %d, keyLen = %d", maxPskLen, keyLen);
        return 0;
    }

    return keyLen;
}

int HdcSSLBase::RsaPrikeyDecrypt(const unsigned char *inBuf, int inLen, unsigned char *outBuf, int outBufLen)
{
    int outLen = -1;
#ifdef HDC_HOST
    outLen = HdcAuth::RsaPrikeyDecryptPsk(inBuf, inLen, outBuf, outBufLen);
#endif
    return outLen;
}

int HdcSSLBase::RsaPubkeyEncrypt(const unsigned char *inBuf, int inLen,
    unsigned char *outBuf, int outBufSize, const string &pubkey)
{
    int outLen = -1;
#ifndef HDC_HOST
    outLen = HdcAuth::RsaPubkeyEncryptPsk(inBuf, inLen, outBuf, outBufSize, pubkey);
#endif
    return outLen;
}

int HdcSSLBase::PerformHandshake(vector<uint8_t> &outBuf)
{
    if (IsHandshakeFinish()) {
        return RET_SSL_HANDSHAKE_FINISHED;
    }
    DoHandshake();
    int nread = GetOutPending();
    if (nread <= 0) {
        WRITE_LOG(LOG_WARN, "SSL PerformHandshake failed, nread = %d", nread);
        return ERR_GENERIC;
    }
    outBuf.resize(nread);
    int outLen = DoBIORead(outBuf.data(), nread);
    if (outLen < 0) {
        WRITE_LOG(LOG_WARN, "BIO_read failed");
        return ERR_GENERIC;
    }
    while (outLen < nread) {
        int tempLen = DoBIORead(outBuf.data() + outLen, nread - outLen);
        if (tempLen > 0) {
            outLen += tempLen;
            WRITE_LOG(LOG_WARN, "PerformHandshake BIO_read left data size %d", tempLen);
        } else if (tempLen == 0) {
            break;
        } else {
            WRITE_LOG(LOG_FATAL, "DoBIORead failed");
            return ERR_GENERIC;
        }
    }
    return RET_SUCCESS;
}

bool HdcSSLBase::SetHandshakeLabel(HSession hSession)
{
    if (!IsHandshakeFinish()) {
        return false;
    }
    ShowSSLInfo();
    hSession->sslHandshake = true;
    return true;
}
} // namespace Hdc
#endif // HDC_SUPPORT_ENCRYPT_TCP