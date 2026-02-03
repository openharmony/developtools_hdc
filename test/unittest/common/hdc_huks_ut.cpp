/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "hdc_huks_ut.h"
#include "securec.h"

#include <filesystem>
#include <fstream>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

using namespace testing::ext;
namespace Hdc {
#define TEST_HUKS_ALIAS "test_huks_alias"
#define TEST_DATA_LEN 10
const uint32_t BUF_SIZE = 4096;

void HdcHuksTest::SetUpTestCase() {}
void HdcHuksTest::TearDownTestCase() {}
void HdcHuksTest::SetUp() {}
void HdcHuksTest::TearDown() {}

int ReadTestKeyFile(uint8_t *keyBuffer, const string &fileName)
{
    (void)memset_s(keyBuffer, BUF_SIZE, 0, BUF_SIZE);

    std::ifstream file(fileName, std::ios::binary);

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    if (size > BUF_SIZE) {
        file.close();
        return 0;
    }

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(keyBuffer), size);
    file.close();
    return size;
}

// 读取 RSA 公钥
RSA* ReadPublicKey(const std::string& filePath)
{
    uint8_t cert[BUF_SIZE];
    int l = ReadTestKeyFile(cert, filePath);

    BIO *b = nullptr;
    b = BIO_new_mem_buf(cert, l);
    RSA* rsa = PEM_read_bio_RSA_PUBKEY(b, nullptr, nullptr, nullptr);
    if (!rsa) {
        rsa = PEM_read_bio_RSAPublicKey(b, nullptr, nullptr, nullptr);
    }

    BIO_free(b);
    return rsa;
}

int GetMaxOaepSha256InputLen(RSA* rsa)
{
    int rsaSize = RSA_size(rsa);

    // OAEP 填充占用：2 字节的 0x00 标记 + 2 * SHA256 哈希长度 (32) + 填充
    // 实际可用数据长度：RSA 密钥长度 - 2 * SHA256 输出长度 - 2
    int maxInputLen = rsaSize - 2 * SHA256_DIGEST_LENGTH - 2;

    return maxInputLen; // 384-64-2 = 318
}

EVP_PKEY_CTX* MakeEvpCtx(EVP_PKEY* pkey, RSA* rsaPubkey)
{
    // 将 RSA 结构体包装为 EVP_PKEY
    if (EVP_PKEY_set1_RSA(pkey, rsaPubkey) != 1) {
        EVP_PKEY_free(pkey);
        return nullptr;
    }

    // 创建加密上下文
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return nullptr;
    }

    // 初始化加密操作
    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return nullptr;
    }

    // 设置使用 OAEP 填充
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return nullptr;
    }

    // 设置 OAEP 使用 SHA256 哈希
    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return nullptr;
    }

    // 设置 MGF1 使用 SHA256
    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return nullptr;
    }

    return ctx;
}

// 使用 RSA-3072 公钥和 OAEP-SHA256 加密数据（分段加密）
int RsaEncryptSegmentedOaepSha256(RSA* rsaPubkey,
    uint8_t* plaintext, int plaintextLen,
    uint8_t* encryptedData, int* totalEncryptedLen)
{
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) {
        return 0;
    }

    EVP_PKEY_CTX* ctx = MakeEvpCtx(pkey, rsaPubkey);
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return 0;
    }

    int segmentSize = GetMaxOaepSha256InputLen(rsaPubkey);
    int rsaSize = RSA_size(rsaPubkey);
    int totalLen = 0;

    int numSegments = (segmentSize == 0) ? 0 : (plaintextLen + segmentSize - 1) / segmentSize;

    for (int i = 0; i < numSegments; i++) {
        int currentSegmentLen = segmentSize;
        if (i == numSegments - 1) {
            currentSegmentLen = plaintextLen - i * segmentSize;
        }

        uint8_t* currentSegment = plaintext + i * segmentSize;
        uint8_t* outputSegment = encryptedData + i * rsaSize;

        size_t outlen;
        if (EVP_PKEY_encrypt(ctx, nullptr, &outlen, currentSegment, currentSegmentLen) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            return 0;
        }

        if (EVP_PKEY_encrypt(ctx, outputSegment, &outlen, currentSegment, currentSegmentLen) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            return 0;
        }

        totalLen += static_cast<int>(outlen);
    }

    *totalEncryptedLen = totalLen;

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return 1;
}

int RsaEncode(const std::string& filePath, const std::string& plainText)
{
    OpenSSL_add_all_algorithms();
    
    RSA* rsaPubkey = ReadPublicKey(filePath);
    if (!rsaPubkey) {
        return 1;
    }

    int rsaSize = RSA_size(rsaPubkey);
    if (rsaSize != 384) {  // 384：RSA_3072密钥大小为384bytes
        return 1;
    }

    int maxSegmentSize = GetMaxOaepSha256InputLen(rsaPubkey);
    int numSegments = (maxSegmentSize == 0) ? 0 : (plainText.size() + maxSegmentSize - 1) / maxSegmentSize;
    int encryptedBufferSize = numSegments * rsaSize;

    uint8_t* encryptedData = (uint8_t*)malloc(encryptedBufferSize);
    if (!encryptedData) {
        RSA_free(rsaPubkey);
        return 1;
    }

    int totalEncryptedLen = 0;
    std::vector<uint8_t> plainTextVector(plainText.begin(), plainText.end());
    if (!RsaEncryptSegmentedOaepSha256(rsaPubkey, plainTextVector.data(), plainTextVector.size(),
        encryptedData, &totalEncryptedLen)) {
        free(encryptedData);
        RSA_free(rsaPubkey);
        return 1;
    }

    std::ofstream outFile("/data/local/tmp/encrypt.txt", std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(reinterpret_cast<const char*>(encryptedData), totalEncryptedLen);
        outFile.close();
        if (!outFile.good()) {
            free(encryptedData);
            RSA_free(rsaPubkey);
            return 1;
        }
    }

    free(encryptedData);
    RSA_free(rsaPubkey);
    return 0;
}

bool ReadEncryptDataFromFile(const std::string& filename, std::vector<uint8_t>& fileData)
{
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile.is_open()) {
        return false;
    }

    inFile.seekg(0, std::ios::end);
    std::streamsize fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    fileData.resize(fileSize);
    inFile.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    inFile.close();

    return true;
}

HWTEST_F(HdcHuksTest, TestResetHuksKey, TestSize.Level0)
{
    HdcHuks huks(TEST_HUKS_ALIAS);
    ASSERT_EQ(huks.ResetHuksKey(), true);
}

HWTEST_F(HdcHuksTest, TestAesGcmEncrypt, TestSize.Level0)
{
    HdcHuks huks(TEST_HUKS_ALIAS);
    std::vector<uint8_t> encryptData;
    uint8_t testData[TEST_DATA_LEN];
    memset_s(testData, TEST_DATA_LEN, '1', TEST_DATA_LEN);
    bool ret = huks.AesGcmEncrypt(testData, TEST_DATA_LEN, encryptData);
    ASSERT_EQ(ret, true);
}

HWTEST_F(HdcHuksTest, TestAesGcmEncryptLen, TestSize.Level0)
{
    HdcHuks huks(TEST_HUKS_ALIAS);
    int len = huks.CaculateGcmEncryptLen(TEST_DATA_LEN);
    ASSERT_EQ(len, 38); // 38 = 12 + 10 + 16
}

HWTEST_F(HdcHuksTest, TestAesGcmEncryptAndDecrypt, TestSize.Level0)
{
    HdcHuks huks(TEST_HUKS_ALIAS);
    std::vector<uint8_t> encryptData;
    uint8_t testData[TEST_DATA_LEN];
    memset_s(testData, TEST_DATA_LEN, '1', TEST_DATA_LEN);
    bool ret = huks.AesGcmEncrypt(testData, TEST_DATA_LEN, encryptData);
    ASSERT_EQ(ret, true);

    std::string encryptStr;
    encryptStr.assign(encryptData.begin(), encryptData.end());
    std::pair<uint8_t*, int> plainStr = huks.AesGcmDecrypt(encryptStr);
    ASSERT_EQ(plainStr.second, 10); // 10 is testData length
    ASSERT_EQ(memcmp(plainStr.first, testData, plainStr.second), 0);
}

HWTEST_F(HdcHuksTest, TestGenerateAndExportHuksRSAPublicKeyFailed, TestSize.Level0)
{
    HdcHuks huks(TEST_HUKS_ALIAS);
    std::filesystem::path testPath = "/data/local/tmp/hdc_test_public_key.pem";
    if (!std::filesystem::exists(testPath))
    {
        std::ofstream file(testPath);
    }
    ASSERT_TRUE(std::filesystem::exists(testPath));
    int32_t result = huks.GenerateAndExportHuksRSAPublicKey();
    ASSERT_EQ(result, -1);
    std::filesystem::remove(testPath);
}

HWTEST_F(HdcHuksTest, TestGenerateAndExportHuksRSAPublicKey, TestSize.Level0)
{
    HdcHuks huks(TEST_HUKS_ALIAS);
    std::filesystem::path testPath = "/data/local/tmp/hdc_test_public_key.pem";
    ASSERT_FALSE(std::filesystem::exists(testPath));
    int32_t result = huks.GenerateAndExportHuksRSAPublicKey();
    ASSERT_EQ(result, HKS_SUCCESS);
    std::filesystem::remove(testPath);
}

HWTEST_F(HdcHuksTest, TestUseRsaPubkeyEncode, TestSize.Level0)
{
    HdcHuks huks(TEST_HUKS_ALIAS);
    std::filesystem::path testPath = "/data/local/tmp/hdc_test_public_key.pem";
    ASSERT_FALSE(std::filesystem::exists(testPath));
    int32_t result = huks.GenerateAndExportHuksRSAPublicKey();
    ASSERT_EQ(result, HKS_SUCCESS);
    
    // 测试加密无需分段加密
    ASSERT_EQ(RsaEncode(testPath, "0123456789abcdefghijklmnopqrstuvwxyz"), 0);
    ASSERT_TRUE(std::filesystem::exists("/data/local/tmp/encrypt.txt"));

    std::vector<uint8_t> encryptData;
    ASSERT_TRUE(ReadEncryptDataFromFile("/data/local/tmp/encrypt.txt", encryptData));

    std::pair<uint8_t*, int> decryptedData = huks.RsaDecryptPrivateKey(encryptData);
    ASSERT_NE(decryptedData.first, nullptr);

    std::string decryptStr(reinterpret_cast<const char*>(decryptedData.first), decryptedData.second);
    delete[] decryptedData.first;
    ASSERT_EQ(decryptStr, "0123456789abcdefghijklmnopqrstuvwxyz");
    std::filesystem::remove("/data/local/tmp/encrypt.txt");

    // 测试分段加解密 dataLen > 318
    std::string testData = "00000000001111111111222222222233333333334444444444"
                           "55555555556666666666777777777788888888889999999999"
                           "aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeee"
                           "ffffffffffgggggggggghhhhhhhhhhiiiiiiiiiijjjjjjjjjj"
                           "kkkkkkkkkkllllllllllmmmmmmmmmmnnnnnnnnnnoooooooooo"
                           "ppppppppppqqqqqqqqqqrrrrrrrrrrsssssssssstttttttttt"
                           "uuuuuuuuuuvvvvvvvvvvwwwwwwwwwwxxxxxxxxxxyyyyyyyyyyzzzzzzzzzz";

    ASSERT_EQ(RsaEncode(testPath, testData), 0);
    ASSERT_TRUE(std::filesystem::exists("/data/local/tmp/encrypt.txt"));

    ASSERT_TRUE(ReadEncryptDataFromFile("/data/local/tmp/encrypt.txt", encryptData));

    decryptedData = huks.RsaDecryptPrivateKey(encryptData);
    ASSERT_NE(decryptedData.first, nullptr);

    std::string decryptDataStr(reinterpret_cast<const char*>(decryptedData.first), decryptedData.second);
    delete[] decryptedData.first;
    ASSERT_EQ(decryptDataStr, testData);

    std::filesystem::remove(testPath);
    std::filesystem::remove("/data/local/tmp/encrypt.txt");
    std::filesystem::remove("/data/local/tmp/hdc_test_public_key.pem");
}

} // namespace Hdc