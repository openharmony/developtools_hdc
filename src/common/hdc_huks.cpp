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
#include "base.h"
#include "credential_message.h"
#include "hdc_huks.h"
#include "hks_param.h"
#include "hks_api.h"
#include "log.h"
#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
#include "os_account_manager.h"
#endif
#include <fstream>

namespace Hdc {
static const int AES_GCM_NONCE_BYTE_LEN = 12;
static const int AES_GCM_TAG_BYTE_LEN = 16;
static const size_t MAX_RSA_CIPHER_TEXT_LEN = 384; // HKS_RSA_KEY_SIZE_3072 / 8
#define HDC_PUBLIC_KEY_PATH "/data/service/el2/public/hdc_service/hdc_public_key.pem"

struct HksParam aesBasePara [] = {
    { .tag = HKS_TAG_ALGORITHM,  .uint32Param = HKS_ALG_AES },
    { .tag = HKS_TAG_KEY_SIZE, .uint32Param = HKS_AES_KEY_SIZE_256 },
    { .tag = HKS_TAG_PADDING, .uint32Param = HKS_PADDING_NONE },
    { .tag = HKS_TAG_BLOCK_MODE, .uint32Param = HKS_MODE_GCM },
    { .tag = HKS_TAG_AUTH_STORAGE_LEVEL, .uint32Param = HKS_AUTH_STORAGE_LEVEL_DE },
};

const struct HksParam rsaBasePara[] = {
    { .tag = HKS_TAG_KEY_STORAGE_FLAG, .uint32Param = HKS_STORAGE_PERSISTENT },
    { .tag = HKS_TAG_ALGORITHM, .uint32Param = HKS_ALG_RSA },
    { .tag = HKS_TAG_KEY_SIZE, .uint32Param = HKS_RSA_KEY_SIZE_3072 },
    { .tag = HKS_TAG_PADDING, .uint32Param =  HKS_PADDING_OAEP },
    { .tag = HKS_TAG_DIGEST, .uint32Param = HKS_DIGEST_SHA256 },
    { .tag = HKS_TAG_AUTH_STORAGE_LEVEL, .uint32Param = HKS_AUTH_STORAGE_LEVEL_DE},
};

HdcHuks::HdcHuks(const std::string& keyAlias)
{
    this->keyAlias = keyAlias;
    this->keyBlobAlias = { static_cast<uint32_t>(this->keyAlias.length()),
                                reinterpret_cast<uint8_t*>(const_cast<char*>(this->keyAlias.c_str())) };
}

#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
static int32_t GetUserId(void)
{
    std::vector<int32_t> ids;

    OHOS::ErrCode err = OHOS::AccountSA::OsAccountManager::QueryActiveOsAccountIds(ids);
    if (err != 0) {
        WRITE_LOG(LOG_FATAL, "QueryActiveOsAccountIds failed, err %d", err);
        return 0;
    }
    if (ids.empty()) {
        WRITE_LOG(LOG_FATAL, "QueryActiveOsAccountIds is empty.");
        return 0;
    }
    return ids[0];
}
#endif
bool HdcHuks::DeleteAesKey(HksParamSet *paramSet)
{
    if (!KeyExist(paramSet)) {
        return true;
    }
    
    int32_t ret = HksDeleteKey(&(this->keyBlobAlias), paramSet);
    if (ret != HKS_SUCCESS) {
        WRITE_LOG(LOG_FATAL, "delete huks key, ret %d", ret);
        return false;
    }
    return true;
}

bool HdcHuks::ResetHuksKey(void)
{
    bool genSuccess = false;

    struct HksParamSet *paramSet = nullptr;
#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
    int32_t currentUserId = GetUserId();
    if (currentUserId  == 0) {
        WRITE_LOG(LOG_FATAL, "current user id is 0, reset key failed.");
        return false;
    }
    WRITE_LOG(LOG_INFO, "current user id %d", currentUserId);
#endif
    struct HksParam genAesKeyPara[] = {
        { .tag = HKS_TAG_PURPOSE, .uint32Param = HKS_KEY_PURPOSE_ENCRYPT | HKS_KEY_PURPOSE_DECRYPT },
#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
        { .tag = HKS_TAG_SPECIFIC_USER_ID, .int32Param = currentUserId },
#endif
    };

    if (!MakeHuksParamSet(&paramSet, aesBasePara, sizeof(aesBasePara) / sizeof(HksParam),
        genAesKeyPara, sizeof(genAesKeyPara) / sizeof(HksParam))) {
        return false;
    }
    do {
        if (!DeleteAesKey(paramSet)) {
            break;
        }
        if (!CreateAesKey(paramSet)) {
            break;
        }
        genSuccess = true;
        WRITE_LOG(LOG_FATAL, "reset key success");
    } while (0);

    HksFreeParamSet(&paramSet);
    return genSuccess;
}

bool HdcHuks::KeyExist(struct HksParamSet *paramSet)
{
    int32_t keyExist = HksKeyExist(&(this->keyBlobAlias), paramSet);
    WRITE_LOG(LOG_INFO, "query key alias, ret %d", keyExist);
    if (keyExist == HKS_SUCCESS) {
        return true;
    }
    return false;
}

bool HdcHuks::CreateAesKey(struct HksParamSet *paramSet)
{
    int32_t ret = HksGenerateKey(&(this->keyBlobAlias), paramSet, nullptr);
    if (ret != HKS_SUCCESS) {
        WRITE_LOG(LOG_FATAL, "generate aes key failed, ret %d", ret);
        return false;
    }
    return true;
}

void HdcHuks::GenerateNonce(uint8_t *nonce, int nonceBufLen)
{
    struct HksBlob nonceBlob = { nonceBufLen, nonce };
    HksGenerateRandom(nullptr, &nonceBlob);
}

struct HksParamSet* HdcHuks::MakeAesGcmEncryptParamSets(uint8_t *nonce, int length)
{
    GenerateNonce(nonce, length);
    struct HksParamSet *paramSet = nullptr;
#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
    int32_t currentUserId = GetUserId();
    if (currentUserId  == 0) {
        WRITE_LOG(LOG_FATAL, "current user id is 0,failed.");
        return nullptr;
    }
    WRITE_LOG(LOG_INFO, "current user id %d", currentUserId);
#endif
    struct HksParam aesEncryptPara[] = {
        { .tag = HKS_TAG_PURPOSE, .uint32Param = HKS_KEY_PURPOSE_ENCRYPT },
        { .tag = HKS_TAG_NONCE, .blob = { length, nonce} },
#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
        { .tag = HKS_TAG_SPECIFIC_USER_ID, .int32Param = currentUserId },
#endif
    };
    if (!MakeHuksParamSet(&paramSet, aesBasePara, sizeof(aesBasePara) / sizeof(HksParam),
        aesEncryptPara, sizeof(aesEncryptPara) / sizeof(HksParam))) {
        return nullptr;
    }
    return paramSet;
}

bool HdcHuks::AesGcmEncrypt(const uint8_t* plainData, int plainDataLen, std::vector<uint8_t>& encryptData)
{
    bool encryptSuccess = true;
    struct HksParamSet *paramSet = nullptr;
    /*
        * encrypt data is : 12 bytes nonce + encrypt text(same length of plain text) + 16 bytes tag
    */
    int encryptBufLen = AES_GCM_NONCE_BYTE_LEN + plainDataLen + AES_GCM_TAG_BYTE_LEN;
    encryptData.resize(encryptBufLen);

    paramSet = MakeAesGcmEncryptParamSets(encryptData.data(), AES_GCM_NONCE_BYTE_LEN);
    if (paramSet == nullptr) {
        return false;
    }
    struct HksBlob plainBlob = {plainDataLen, const_cast<uint8_t*>(plainData)};
    struct HksBlob encryptBlob = {encryptBufLen - AES_GCM_NONCE_BYTE_LEN,
                                  encryptData.data() + AES_GCM_NONCE_BYTE_LEN};
    int32_t ret = HksEncrypt(&(this->keyBlobAlias), paramSet, &plainBlob, &encryptBlob);
    if (ret != HKS_SUCCESS) {
        WRITE_LOG(LOG_FATAL, "HksEncrypt failed, ret %d", ret);
        encryptSuccess = false;
    }

    WRITE_LOG(LOG_FATAL, "HksEncrypt encryptData len is %d", encryptData.size());
    HksFreeParamSet(&paramSet);
    return encryptSuccess;
}

struct HksParamSet* HdcHuks::MakeAesGcmDecryptParamSets(std::vector<uint8_t>& nonce)
{
    struct HksParamSet *paramSet = nullptr;
    
#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
    int32_t currentUserId = GetUserId();
    if (currentUserId  == 0) {
        WRITE_LOG(LOG_FATAL, "current user id is 0,failed.");
        return nullptr;
    }
    WRITE_LOG(LOG_INFO, "current user id %d", currentUserId);
#endif
    struct HksParam aesDecryptPara[] = {
        { .tag = HKS_TAG_PURPOSE, .uint32Param = HKS_KEY_PURPOSE_DECRYPT },
        { .tag = HKS_TAG_NONCE, .blob = { nonce.size(), nonce.data()} },
#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
        { .tag = HKS_TAG_SPECIFIC_USER_ID, .int32Param = currentUserId },
#endif

    };
    if (!MakeHuksParamSet(&paramSet, aesBasePara, sizeof(aesBasePara) / sizeof(HksParam),
        aesDecryptPara, sizeof(aesDecryptPara) / sizeof(HksParam))) {
        return nullptr;
    }
    return paramSet;
}

bool HdcHuks::CheckEncryptDataLen(const std::string& encryptData)
{
    if (encryptData.length() < AES_GCM_NONCE_BYTE_LEN + AES_GCM_TAG_BYTE_LEN) {
        WRITE_LOG(LOG_FATAL, "invalid data len %zd", encryptData.length());
        return false;
    }
    return true;
}

/*
    * input data is 12 bytes(nonce value) + N bytes(encrypt data + 16 bytes(tag value))
*/
std::pair<uint8_t*, int> HdcHuks::AesGcmDecrypt(const std::string& inputData)
{
    struct HksParamSet *paramSet = nullptr;
    std::vector<uint8_t> encryptText;
    std::vector<uint8_t> nonceValue;

    if (!CheckEncryptDataLen(inputData)) {
        return std::make_pair(nullptr, 0);
    }

    nonceValue.assign(inputData.begin(), inputData.begin() + AES_GCM_NONCE_BYTE_LEN);
    encryptText.assign(inputData.begin() + AES_GCM_NONCE_BYTE_LEN, inputData.end());
    paramSet = MakeAesGcmDecryptParamSets(nonceValue);
    if (paramSet == nullptr) {
        return std::make_pair(nullptr, 0);
    }

    size_t maxPlainDataLen = inputData.size() - AES_GCM_NONCE_BYTE_LEN;
    uint8_t *plainData = new(std::nothrow)uint8_t[maxPlainDataLen];
    if (plainData == nullptr) {
        WRITE_LOG(LOG_FATAL, " out of memory %zu", maxPlainDataLen);
        HksFreeParamSet(&paramSet);
        return std::make_pair(nullptr, 0);
    }

    struct HksBlob encryptBlob = { encryptText.size(), encryptText.data() };
    struct HksBlob plainBlob = { maxPlainDataLen, plainData};
    int32_t ret = HksDecrypt(&(this->keyBlobAlias), paramSet, &encryptBlob, &plainBlob);
    if (ret != HKS_SUCCESS) {
        WRITE_LOG(LOG_FATAL, "HksDecrypt failed, ret %d", ret);
        delete[] plainData;
        HksFreeParamSet(&paramSet);
        return std::make_pair(nullptr, 0);
    }

    HksFreeParamSet(&paramSet);
    return std::make_pair(plainData, plainBlob.size);
}

bool HdcHuks::MakeHuksParamSet(struct HksParamSet **paramSet,
    const struct HksParam *baseParams, int baseParamCount, const struct HksParam *params, int paramCount)
{
    bool buildSuccess = true;
    
    int32_t ret = HksInitParamSet(paramSet);
    if (ret != HKS_SUCCESS) {
        WRITE_LOG(LOG_FATAL, "HksInitParamSet failed, ret %d", ret);
        return false;
    }
    do {
        ret = HksAddParams(*paramSet, baseParams, baseParamCount);
        if (ret != HKS_SUCCESS) {
            WRITE_LOG(LOG_FATAL, "add base param failed, ret %d", ret);
            buildSuccess = false;
            break;
        }
        ret = HksAddParams(*paramSet, params, paramCount);
        if (ret != HKS_SUCCESS) {
            WRITE_LOG(LOG_FATAL, "add param failed, ret %d", ret);
            buildSuccess = false;
            break;
        }
        ret = HksBuildParamSet(paramSet);
        if (ret != HKS_SUCCESS) {
            WRITE_LOG(LOG_FATAL, "HksBuildParamSet failed, ret %d", ret);
            buildSuccess = false;
            break;
        }
    } while (0);

    if (!buildSuccess) {
        HksFreeParamSet(paramSet);
    }
    return buildSuccess;
}

int HdcHuks::CaculateGcmEncryptLen(int palinDataLen)
{
    return palinDataLen + AES_GCM_NONCE_BYTE_LEN + AES_GCM_TAG_BYTE_LEN;
}

std::string HdcHuks::base64Encode(const std::vector<unsigned char> &data)
{
    BIO *bio = nullptr;
    BIO *b64 = nullptr;
    BUF_MEM *bufferPtr = nullptr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, data.data(), data.size());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    BIO_set_close(bio, BIO_NOCLOSE);
    BIO_free_all(bio);

    std::string ret(bufferPtr->data, bufferPtr->length);
    BUF_MEM_free(bufferPtr);
    return ret;
}

int32_t HdcHuks::ExportRsaHksExportPublicKey(const struct HksParamSet *paramSetIn)
{
    int32_t result = -1;
    std::vector<uint8_t> publicKeyData(HKS_RSA_KEY_SIZE_3072);
    struct HksBlob publicKey = {.size = HKS_RSA_KEY_SIZE_3072, .data = publicKeyData.data()};
    result = HksExportPublicKey(&keyBlobAlias, paramSetIn, &publicKey);
    if (result != HKS_SUCCESS) {
        WRITE_LOG(LOG_FATAL, "Failed to export public key, result: %d", result);
        return -1;
    }

    std::vector<unsigned char> vec(publicKey.size);
    if (publicKey.size > 0 && publicKey.data != nullptr) {
        std::copy(publicKey.data, publicKey.data + publicKey.size, vec.begin());
    }
    std::string base64Data = base64Encode(vec);
    std::ofstream file(HDC_PUBLIC_KEY_PATH);
    file << "-----BEGIN PUBLIC KEY-----\n";
    file << base64Data;
    file << "-----END PUBLIC KEY-----\n";
    file.close();

    delete[] publicKey.data;
    return result;
}

int32_t HdcHuks::GenerateAndExportHuksRSAPublicKey()
{
    struct stat status;
    if (stat(HDC_PUBLIC_KEY_PATH, &status) == 0) {
        WRITE_LOG(LOG_FATAL, "HuksRSAPublicKey exist");
        return -1;
    }
    struct HksParamSet *generateParamSet = nullptr;
    int32_t result = -1;

#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
    int32_t currentUserId = GetUserId();
    if (currentUserId == 0) {
        WRITE_LOG(LOG_FATAL, "current user id is 0,failed.");
        return result;
    }
    WRITE_LOG(LOG_INFO, "current user id %d", currentUserId);
#endif
    struct HksParam genRsaKeyPara[] = {
        {.tag = HKS_TAG_PURPOSE, .uint32Param = HKS_KEY_PURPOSE_ENCRYPT | HKS_KEY_PURPOSE_DECRYPT},
#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
        {.tag = HKS_TAG_SPECIFIC_USER_ID, .int32Param = currentUserId},
#endif
    };

    if (!MakeHuksParamSet(&generateParamSet, rsaBasePara, sizeof(rsaBasePara) / sizeof(HksParam),
        genRsaKeyPara, sizeof(genRsaKeyPara) / sizeof(HksParam))) {
        return result;
    }

    result = HksGenerateKey(&(this->keyBlobAlias), generateParamSet, nullptr);
    if (result != HKS_SUCCESS) {
        WRITE_LOG(LOG_FATAL, "Failed to HksGenerateKey, result: ", result);
        return result;
    }

    result = ExportRsaHksExportPublicKey(generateParamSet);

    HksFreeParamSet(&generateParamSet);
    return result;
}

struct HksParamSet *HdcHuks::MakeRsaDecryptParamSets()
{
    struct HksParamSet *paramSet = nullptr;

#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
    int32_t currentUserId = GetUserId();
    if (currentUserId == 0) {
        WRITE_LOG(LOG_FATAL, "current user id is 0,failed.");
        return nullptr;
    }
    WRITE_LOG(LOG_INFO, "current user id %d", currentUserId);
#endif
    struct HksParam rsaDecryptPara[] = {
        {.tag = HKS_TAG_PURPOSE, .uint32Param = HKS_KEY_PURPOSE_DECRYPT},
#if !defined(HDC_HOST) && !defined(HDC_UNIT_TEST)
        {.tag = HKS_TAG_SPECIFIC_USER_ID, .int32Param = currentUserId},
#endif
    };
    if (!MakeHuksParamSet(&paramSet, rsaBasePara, sizeof(rsaBasePara) / sizeof(HksParam),
        rsaDecryptPara, sizeof(rsaDecryptPara) / sizeof(HksParam))) {
        return nullptr;
    }
    return paramSet;
}

std::pair<uint8_t *, int> HdcHuks::RsaDecryptPrivateKey(std::vector<uint8_t> &inputData)
{
    const size_t totalSize = inputData.size();
    if (totalSize == 0) {
        WRITE_LOG(LOG_FATAL, "inputData size %d ", inputData.size());
        return std::make_pair(nullptr, 0);
    }

    std::vector<uint8_t> decryptedData;
    decryptedData.reserve(totalSize);

    struct HksParamSet *paramSet = nullptr;
    paramSet = MakeRsaDecryptParamSets();
    if (paramSet == nullptr) {
        WRITE_LOG(LOG_FATAL, "MakeRsaDecryptParamSets failed");
        return std::make_pair(nullptr, 0);
    }

    for (size_t offset = 0; offset < totalSize; offset += MAX_RSA_CIPHER_TEXT_LEN) {
        const size_t currentSegmentSize = std::min(MAX_RSA_CIPHER_TEXT_LEN, totalSize - offset);
        const auto segmentStart = inputData.begin() + offset;

        struct HksBlob encryptBlob = {currentSegmentSize, reinterpret_cast<uint8_t*>(&(*segmentStart))};

        std::unique_ptr<uint8_t[]> plainData = std::make_unique<uint8_t[]>(currentSegmentSize);
        struct HksBlob plainBlob = {currentSegmentSize, plainData.get()};

        int32_t ret = HksDecrypt(&(this->keyBlobAlias), paramSet, &encryptBlob, &plainBlob);
        if (ret != HKS_SUCCESS) {
            std::string str(reinterpret_cast<char*>(this->keyBlobAlias.data));
            WRITE_LOG(LOG_FATAL, "HksDecrypt failed, key %s, ret %d", str.c_str(), ret);
            HksFreeParamSet(&paramSet);
            return std::make_pair(nullptr, 0);
        }

        decryptedData.insert(decryptedData.end(), plainData.get(), plainData.get() + plainBlob.size);
    }

    auto resultData = std::make_unique<uint8_t[]>(decryptedData.size());
    std::copy(decryptedData.begin(), decryptedData.end(), resultData.get());

    WRITE_LOG(LOG_FATAL, "HksDecrypt Successed, size %zu", decryptedData.size());
    HksFreeParamSet(&paramSet);
    return std::make_pair(resultData.release(), decryptedData.size());
}

} // namespace Hdc