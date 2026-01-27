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

#include "connect_validation.h"
#include "sys/socket.h"
#include <parameters.h>
#include <regex>

using namespace Hdc;
using namespace HdcAuth;

namespace HdcValidation {

static const std::string DAEMON_PUBLICKEY_PATH = "/data/service/el2/public/hdc_service/verify_public_key.pem";

int GetConnectValidationParam()
{
    std::string parameterValue = OHOS::system::GetParameter("persist.hdc.control.enterprise_connect_validation", "");
    if (parameterValue.empty()) {
        WRITE_LOG(LOG_FATAL, "Parameter not found or value is empty.");
        return VALIDATION_CLOSE;
    }

    std::regex pattern("^(0|[1-9]\\d?|1\\d\\d|2[0-4]\\d|25[0-5])$");
    if (!std::regex_match(parameterValue, pattern)) {
        return VALIDATION_CLOSE;
    }

    uint8_t result = VALIDATION_CLOSE;
    result = static_cast<uint8_t>(std::stoul(parameterValue));
    if (result > VALIDATION_HDC_HOST_AND_DAEMON) {
        result = VALIDATION_CLOSE;
    }

    return result;
}

#ifdef HDC_HOST
bool GetPubKeyHash(string &pubkeyInfo)
{
    string str = "GetPubkey";
    std::string sendStr = SplicMessageStr(str, GET_PUBKEY, METHOD_AUTHVERIFY);
    if (sendStr.empty()) {
        WRITE_LOG(LOG_FATAL, "sendStr is empty.");
        return false;
    }
    char data[MESSAGE_STR_MAX_LEN] = {0};
    ssize_t count = GetCredential(sendStr, data, MESSAGE_STR_MAX_LEN);
    memset_s(sendStr.data(), sendStr.size(), 0, sendStr.size());
    if (count <= 0) {
        WRITE_LOG(LOG_FATAL, "GetPubkey is empty.");
        return false;
    }
    std::string recvStr(data, count);
    CredentialMessage messageStruct(recvStr);
    memset_s(recvStr.data(), recvStr.size(), 0, recvStr.size());
    memset_s(data, sizeof(data), 0, sizeof(data));
    switch (messageStruct.GetMessageMethodType()) {
        case GET_PUBKEY_FAILED:
            pubkeyInfo = "[E000008]: The sdk hdc.exe failed to read the public key.";
            break;
        case GET_PRIVKEY_FAILED:
            pubkeyInfo = "[E000009]: The sdk hdc.exe failed to read the private key.";
            break;
        case MISMATCH_PUBKEY_PRIVKEY:
            pubkeyInfo = "[E00000A]: The sdk hdc.exe public and private keys do not match.";
            break;
        default:
            break;
    }
    if (messageStruct.GetMessageBodyLen() > 0) {
        pubkeyInfo = messageStruct.GetMessageBody();
        return true;
    } else {
        WRITE_LOG(LOG_FATAL, "Error: messageBodyLen is 0.");
        return false;
    }
    return true;
}

bool GetPublicKeyHashInfo(string &buf)
{
    string hostname;
    if (!HdcAuth::GetHostName(hostname)) {
        WRITE_LOG(LOG_FATAL, "gethostname failed");
        return false;
    }

    std::string pubkeyInfo;
    if (!GetPubKeyHash(pubkeyInfo)) {
        string msg = pubkeyInfo;
        Base::TlvAppend(buf, TAG_EMGMSG, msg);
        Base::TlvAppend(buf, TAG_DAEOMN_AUTHSTATUS, DAEOMN_UNAUTHORIZED);
        return false;
    }

    buf = hostname;
    buf.append(HDC_HOST_DAEMON_BUF_SEPARATOR);
    buf.append(pubkeyInfo);

    WRITE_LOG(LOG_INFO, "Get pubkey info success");

    return true;
}

bool GetPrivateKeyInfo(string &privkey_info)
{
    string str = "GetPrivateKey";
    std::string sendStr = SplicMessageStr(str, GET_SIGNATURE, METHOD_AUTHVERIFY);
    if (sendStr.empty()) {
        WRITE_LOG(LOG_FATAL, "sendStr is empty.");
        return false;
    }
    char data[MESSAGE_STR_MAX_LEN] = {0};
    ssize_t count = GetCredential(sendStr, data, MESSAGE_STR_MAX_LEN);
    memset_s(sendStr.data(), sendStr.size(), 0, sendStr.size());
    if (count <= 0) {
        WRITE_LOG(LOG_FATAL, "GetPrivateKey is empty.");
        return false;
    }
    std::string recvStr(data, count);
    CredentialMessage messageStruct(recvStr);
    memset_s(recvStr.data(), recvStr.size(), 0, recvStr.size());
    memset_s(data, sizeof(data), 0, sizeof(data));
    switch (messageStruct.GetMessageMethodType()) {
        case GET_PRIVKEY_FAILED:
            privkey_info = "[E000009]: The sdk hdc.exe failed to read the private key.";
            break;
        default:
            break;
    }
    if (messageStruct.GetMessageBodyLen() > 0) {
        privkey_info = messageStruct.GetMessageBody();
        return true;
    } else {
        WRITE_LOG(LOG_FATAL, "Error: messageBodyLen is 0.");
        return false;
    }
    return true;
}

EVP_PKEY *StringToEVP(const std::string &pemStr)
{
    BIO *bio = BIO_new_mem_buf(pemStr.c_str(), pemStr.length());
    if (!bio) {
        return nullptr;
    }

    EVP_PKEY *evp = nullptr;
    evp = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    if (!evp) {
        BIO_reset(bio);
        RSA *rsa = PEM_read_bio_RSAPrivateKey(bio, nullptr, nullptr, nullptr);
        if (rsa) {
            evp = EVP_PKEY_new();
            if (!EVP_PKEY_assign_RSA(evp, rsa)) {
                RSA_free(rsa);
                evp = nullptr;
            }
        }
    }

    BIO_free(bio);
    return evp;
}

bool RsaSignAndBase64(string &buf, Hdc::AuthVerifyType type, string &pemStr)
{
    bool signResult = false;
    EVP_PKEY *evp = StringToEVP(pemStr);
    if (evp == nullptr) {
        WRITE_LOG(LOG_FATAL, "Failed to parse PEM key");
        return signResult;
    }
    RSA *rsa = nullptr;
    rsa = EVP_PKEY_get1_RSA(evp);

    if (type == Hdc::AuthVerifyType::RSA_3072_SHA512) {
        signResult = HdcAuth::RsaSign(buf, evp);
    } else {
        signResult = HdcAuth::RsaEncrypt(buf, rsa);
    }

    if (rsa != nullptr) {
        RSA_free(rsa);
    }
    if (evp != nullptr) {
        EVP_PKEY_free(evp);
    }

    return signResult;
}

#else

std::vector<EVP_PKEY*> ReadMultiplePublicKeys()
{
    std::vector<EVP_PKEY*> keys;
    
    FILE* fp = fopen(DAEMON_PUBLICKEY_PATH.c_str(), "r");
    if (!fp) {
        WRITE_LOG(LOG_FATAL, "Failed to open key file");
        return keys;
    }

    BIO* bio = BIO_new_fp(fp, BIO_NOCLOSE);
    if (!bio) {
        WRITE_LOG(LOG_FATAL, "Failed to create BIO object");
        (void)fclose(fp);
        return keys;
    }
    
    EVP_PKEY* pkey = nullptr;
    while ((pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr)) != nullptr) {
        keys.push_back(pkey);
    }

    BIO_free(bio);
    (void)fclose(fp);
    
    return keys;
}

bool GetPublicKeyFingerprint(EVP_PKEY* pubKey, uint8_t* publicKeyHash)
{
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pubKey, nullptr);
    if (!pctx) {
        WRITE_LOG(LOG_WARN, "Failed to create EVP_PKEY_CTX");
        return false;
    }

    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        WRITE_LOG(LOG_WARN, "Failed to initialize key generation context");
        EVP_PKEY_CTX_free(pctx);
        return false;
    }

    EVP_PKEY_CTX_free(pctx);

    unsigned char *pubKeyData = nullptr;
    int pubKeyLen = i2d_PUBKEY(pubKey, &pubKeyData);
    if (pubKeyLen <= 0) {
        WRITE_LOG(LOG_WARN, "Failed to convert public key to data");
        return false;
    }

    if (SHA256(pubKeyData, pubKeyLen, publicKeyHash) == nullptr) {
        WRITE_LOG(LOG_WARN, "SHA256 calculation of public key failed");
        OPENSSL_free(pubKeyData);
        return false;
    }

    OPENSSL_free(pubKeyData);

    return true;
}

bool EvpkeyConvert2String(EVP_PKEY* key, string &pubkey)
{
    bool ret = false;
    BIO *bio = nullptr;
    do {
        bio = BIO_new(BIO_s_mem());
        if (!bio) {
            ret = false;
            WRITE_LOG(LOG_FATAL, "alloc bio mem failed");
            break;
        }
        if (!PEM_write_bio_PUBKEY(bio, key)) {
            ret = false;
            WRITE_LOG(LOG_FATAL, "write bio failed");
            break;
        }
        size_t len = 0;
        char buf[RSA_KEY_BITS] = {0};
        if (BIO_read_ex(bio, buf, sizeof(buf), &len) <= 0) {
            ret = false;
            WRITE_LOG(LOG_FATAL, "read bio failed");
            break;
        }
        pubkey = string(buf, len);
        ret = true;
        WRITE_LOG(LOG_INFO, "load pubkey success");
    } while (0);

    if (bio) {
        BIO_free(bio);
        bio = nullptr;
    }

    return ret;
}

bool CheckAuthPubKeyIsValid(string &pubkeyHash, string &pubkey)
{
    bool ret = false;
    uint8_t publicKeyHash[SHA256_DIGEST_LENGTH] = {0};
    string keyHashInfo;
    auto keys = ReadMultiplePublicKeys();
    if (keys.empty()) {
        WRITE_LOG(LOG_INFO, "ReadMultiplePublicKeys failed");
        return false;
    }

    for (EVP_PKEY* key : keys) {
        memset_s(publicKeyHash, SHA256_DIGEST_LENGTH, 0, SHA256_DIGEST_LENGTH);
        // get key hash
        if (!GetPublicKeyFingerprint(key, publicKeyHash)) {
            continue;
        }
        keyHashInfo = Base::Convert2HexStr(publicKeyHash, SHA256_DIGEST_LENGTH);
        if (0 == keyHashInfo.compare(pubkeyHash)) {
            ret = EvpkeyConvert2String(key, pubkey);
            break;
        }
    }

    for (EVP_PKEY* key : keys) {
        EVP_PKEY_free(key);
    }
    
    return ret;
}
#endif
}  // namespace HdcValidation