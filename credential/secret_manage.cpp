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

#include "auth.h"
#include "credential_message.h"
#include "log.h"
#include "secret_manage.h"

using namespace Hdc;

namespace {
static const std::string VERIFY_PUBLIC_KEY_PATH = "/data/service/el2/public/hdc_service/verify_public_key.pem";
static const std::string ENCRYPT_PRIVATE_KEY_PATH = "/data/service/el4/100/hdc_service/encrypt_private_key.pem";
}  // namespace

HdcSecretManage::HdcSecretManage(const std::string &keyAlias):hdcRsaHuks(keyAlias)
{
}

uint8_t *HdcSecretManage::GetPublicKeyHash()
{
    return publicKeyHash;
}

bool HdcSecretManage::ReadEncryptKeyFile(std::vector<uint8_t>& fileData)
{
    std::ifstream inFile(ENCRYPT_PRIVATE_KEY_PATH, std::ios::binary);
    if (!inFile.is_open()) {
        WRITE_LOG(LOG_FATAL, "open private key failed");
        return false;
    }
    inFile.seekg(0, std::ios::end);
    std::streamsize fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    if (fileSize == 0) {
        WRITE_LOG(LOG_FATAL, "the private key is empty.");
        return false;
    }
    
    fileData.resize(fileSize);
    inFile.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    if (inFile.eof() || inFile.fail()) {
        WRITE_LOG(LOG_FATAL, "read file private key failed");
        inFile.close();
        return false;
    }
    inFile.close();
    return true;
}

bool HdcSecretManage::DerToEvpPkey(const std::pair<uint8_t*, int>& privKeyInfo)
{
    std::string pemStr(reinterpret_cast<const char*>(privKeyInfo.first), privKeyInfo.second);

    BIO* bio = BIO_new_mem_buf(pemStr.c_str(), pemStr.size());
    if (!bio) {
        WRITE_LOG(LOG_FATAL, "Failed to create BIO");
        return false;
    }

    privKey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    if (!privKey) {
        WRITE_LOG(LOG_FATAL, "Failed to parse PEM private key");
        return false;
    }

    BIO_free(bio);
    return true;
}

bool HdcSecretManage::LoadPrivateKeyInfo()
{
    WRITE_LOG(LOG_FATAL, "LoadPrivateKeyInfo");
    std::vector<uint8_t> encryptData;

    if (!ReadEncryptKeyFile(encryptData)) {
        return false;
    }
    std::pair<uint8_t*, int> privKeyInfo = hdcRsaHuks.RsaDecryptPrivateKey(encryptData);
    if (privKeyInfo.first == nullptr) {
        WRITE_LOG(LOG_FATAL, "RsaDecryptPrivateKey failed.");
        return false;
    }

    bool ret = DerToEvpPkey(privKeyInfo);

    if (memset_s(privKeyInfo.first, privKeyInfo.second, 0, privKeyInfo.second) != EOK) {
        WRITE_LOG(LOG_FATAL, "memset_s failed.");
    }
    delete[] privKeyInfo.first;

    return ret;
}

void HdcSecretManage::ClearPrivateKeyInfo()
{
    if (!privKey) {
        return;
    }
    EVP_PKEY_free(privKey);
}

void HdcSecretManage::LoadPublicKeyInfo()
{
    FILE *pubFile = fopen(VERIFY_PUBLIC_KEY_PATH.c_str(), "r");
    if (!pubFile) {
        return;
    }

    pubKey = PEM_read_PUBKEY(pubFile, nullptr, nullptr, nullptr);
    (void)fclose(pubFile);
}

void HdcSecretManage::ClearPublicKeyInfo()
{
    if (!pubKey) {
        return;
    }
    EVP_PKEY_free(pubKey);
}

bool HdcSecretManage::GetPublicKeyFingerprint()
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

bool HdcSecretManage::SignatureByPrivKey(const char *testData, std::vector<unsigned char> &signature, size_t &reqLen)
{
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return false;
    }

    if (1 != EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, privKey)) {
        WRITE_LOG(LOG_WARN, "EVP_DigestSignInit failed");
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    reqLen = 0;
    if (1 != EVP_DigestSign(mdctx, nullptr, &reqLen, (const unsigned char *)testData, strlen(testData))) {
        WRITE_LOG(LOG_WARN, "EVP_DigestSign failed");
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    signature.resize(reqLen);
    if (1 != EVP_DigestSign(mdctx, signature.data(), &reqLen, (const unsigned char *)testData, strlen(testData))) {
        WRITE_LOG(LOG_WARN, "EVP_DigestSign failed");
        EVP_MD_CTX_free(mdctx);
        return false;
    }
    EVP_MD_CTX_free(mdctx);
    return true;
}

bool HdcSecretManage::VerifyByPublicKey(const char *testData,
    std::vector<unsigned char> &signature, const size_t reqLen)
{
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return false;
    }

    if (1 != EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pubKey)) {
        WRITE_LOG(LOG_WARN, "EVP_DigestVerifyInit failed");
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    int result = EVP_DigestVerify(mdctx, signature.data(), reqLen, (const unsigned char *)testData, strlen(testData));
    EVP_MD_CTX_free(mdctx);
    return result == 1;
}

bool HdcSecretManage::CheckPubkeyAndPrivKeyMatch()
{
    LoadPublicKeyInfo();
    if (!LoadPrivateKeyInfo()) {
        WRITE_LOG(LOG_FATAL, "LoadPrivateKeyInfo failed.");
        return false;
    }

    if (!pubKey || !privKey) {
        return false;
    }

    /* Encrypt the data using private key and decrypt the data using the public key to check
       whether the public and private keys match each other. */
    const char *testData = "Test data for key matching";
    std::vector<unsigned char> signature;
    size_t reqLen = 0;

    if (!SignatureByPrivKey(testData, signature, reqLen)) {
        WRITE_LOG(LOG_WARN, "SignatureByPrivKey failed");
        ClearPublicKeyInfo();
        ClearPrivateKeyInfo();
        return false;
    }

    bool ret = VerifyByPublicKey(testData, signature, reqLen);

    ClearPublicKeyInfo();
    ClearPrivateKeyInfo();
    return ret;
}

bool HdcSecretManage::LoadPublicKeyHash()
{
    LoadPublicKeyInfo();
    if (!pubKey) {
        WRITE_LOG(LOG_FATAL, "load key failed");
        return false;
    }

    bool ret = GetPublicKeyFingerprint();
    ClearPublicKeyInfo();
    return ret;
}

int HdcSecretManage::TryLoadPublicKeyInfo()
{
    struct stat status;
    if (stat(VERIFY_PUBLIC_KEY_PATH.c_str(), &status) == -1) {
        WRITE_LOG(LOG_FATAL, "The pubkey file does not exist.");
        return GET_PUBKEY_FAILED;
    }

    if (stat(ENCRYPT_PRIVATE_KEY_PATH.c_str(), &status) == -1) {
        WRITE_LOG(LOG_FATAL, "The privkey file does not exist or is inaccessible: %s", strerror(errno));
        return GET_PRIVKEY_FAILED;
    }

    if (!CheckPubkeyAndPrivKeyMatch()) {
        WRITE_LOG(LOG_FATAL, "public and private keys do not match.");
        return MISMATCH_PUBKEY_PRIVKEY;
    }

    if (!LoadPublicKeyHash()) {
        WRITE_LOG(LOG_FATAL, "load Pubkey failed");
        return GET_PUBKEY_FAILED;
    }

    return GET_PUBKEY_SUCCESSED;
}

int HandleGetPubkeyMessage(std::string &processMessageValue)
{
    std::shared_ptr<HdcSecretManage> secretManage = std::make_shared<HdcSecretManage>(HDC_RAS_KEY_FILE_PWD_KEY_ALIAS);
    int ret = secretManage->TryLoadPublicKeyInfo();
    if (GET_PUBKEY_SUCCESSED != ret) {
        WRITE_LOG(LOG_FATAL, "LoadPublicKey failed.");
        return ret;
    }
    auto sha256Result = secretManage->GetPublicKeyHash();
    processMessageValue = Base::Convert2HexStr(sha256Result, SHA256_DIGEST_LENGTH);
    return GET_PUBKEY_SUCCESSED;
}

int HdcSecretManage::TryLoadPrivateKeyInfo(std::string &processMessageValue)
{
    struct stat status;
    if (stat(ENCRYPT_PRIVATE_KEY_PATH.c_str(), &status) == -1) {
        WRITE_LOG(LOG_FATAL, "The privkey file does not exist.");
        return GET_PRIVKEY_FAILED;
    }

    if (!LoadPrivateKeyInfo()) {
        WRITE_LOG(LOG_FATAL, "LoadPrivateKeyInfo failed.");
        return false;
    }
    if (!privKey) {
        WRITE_LOG(LOG_FATAL, "load privkey failed");
        return GET_PRIVKEY_FAILED;
    }

    int ret = GET_PRIVATE_SUCCESSED;
    BIO *bio = nullptr;
    do {
        bio = BIO_new(BIO_s_mem());
        if (!bio) {
            ret = GET_PRIVKEY_FAILED;
            WRITE_LOG(LOG_FATAL, "alloc bio mem failed");
            break;
        }
        if (!PEM_write_bio_PrivateKey(bio, privKey, nullptr, nullptr, 0, nullptr, nullptr)) {
            ret = GET_PRIVKEY_FAILED;
            WRITE_LOG(LOG_FATAL, "write bio failed");
            break;
        }
        size_t len = 0;
        char buf[RSA_KEY_BITS] = {0};
        if (BIO_read_ex(bio, buf, sizeof(buf), &len) <= 0) {
            ret = GET_PRIVKEY_FAILED;
            WRITE_LOG(LOG_FATAL, "read bio failed");
            break;
        }
        processMessageValue = string(buf, len);
    } while (0);

    if (bio) {
        BIO_free(bio);
        bio = nullptr;
    }
    return GET_PRIVATE_SUCCESSED;
}

int HandleGetSignatureMessage(std::string &processMessageValue)
{
    std::shared_ptr<HdcSecretManage> secretManage = std::make_shared<HdcSecretManage>(HDC_RAS_KEY_FILE_PWD_KEY_ALIAS);
    int ret = secretManage->TryLoadPrivateKeyInfo(processMessageValue);
    return ret;
}