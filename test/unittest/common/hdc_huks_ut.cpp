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

using namespace testing::ext;
namespace Hdc {
#define TEST_HUKS_ALIAS "test_huks_alias"
#define TEST_DATA_LEN 10
void HdcHuksTest::SetUpTestCase() {}
void HdcHuksTest::TearDownTestCase() {}
void HdcHuksTest::SetUp() {}
void HdcHuksTest::TearDown() {}

void HdcHuksTest::ReaEncode(const std::string& plainStr) 
{
    
}

// 读取 RSA 公钥
RSA* read_public_key(const char* filename)
{
    unsigned char cert[BUF_SIZE];
    int l = readFile_1(cert, filename);

    BIO *b;
    b = BIO_new_mem_buf(cert, l);

    RSA* rsa = PEM_read_bio_RSA_PUBKEY(b, NULL, NULL, NULL);
    if (!rsa) {
        // 尝试另一种格式
        rsa = PEM_read_bio_RSAPublicKey(b, NULL, NULL, NULL);
    }

    if (!rsa) {
        fprintf(stderr, "无法读取公钥\n");
        ERR_print_errors_fp(stderr);
    }

    return rsa;
}

// 计算 RSA-OAEP-SHA256 的最大加密数据长度
int get_max_oaep_sha256_input_len(RSA* rsa)
{
    int rsa_size = RSA_size(rsa);	//384

    // OAEP 填充占用：2 字节的 0x00 标记 + 2 * SHA256 哈希长度 (32) + 填充
    // 实际可用数据长度：RSA 密钥长度 - 2 * SHA256 输出长度 - 2
    int max_input_len = rsa_size - 2 * SHA256_DIGEST_LENGTH - 2;

    printf("RSA 密钥长度: %d 字节\n", rsa_size);
    printf("SHA256 摘要长度: %d 字节\n", SHA256_DIGEST_LENGTH);
    printf("OAEP-SHA256 最大输入数据长度: %d 字节\n", max_input_len);

    return max_input_len; // 384-64-2 = 318
}

// 使用 RSA-3072 公钥和 OAEP-SHA256 加密数据（分段加密）
int rsa_encrypt_segmented_oaep_sha256(RSA* rsa_pubkey,
    unsigned char* plaintext, int plaintext_len,
    unsigned char* encrypted_data, int* total_encrypted_len,
    int segment_size)
{
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) {
        fprintf(stderr, "EVP_PKEY_new 失败\n");
        return 0;
    }

    // 将 RSA 结构体包装为 EVP_PKEY
    if (EVP_PKEY_set1_RSA(pkey, rsa_pubkey) != 1) {
        fprintf(stderr, "EVP_PKEY_set1_RSA 失败\n");
        EVP_PKEY_free(pkey);
        return 0;
    }

    // 创建加密上下文
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx) {
        fprintf(stderr, "EVP_PKEY_CTX_new 失败\n");
        EVP_PKEY_free(pkey);
        return 0;
    }

    // 初始化加密操作
    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        fprintf(stderr, "EVP_PKEY_encrypt_init 失败\n");
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return 0;
    }

    // 设置使用 OAEP 填充
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
        fprintf(stderr, "设置 RSA OAEP 填充失败\n");
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return 0;
    }

    // 设置 OAEP 使用 SHA256 哈希
    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) {
        fprintf(stderr, "设置 OAEP SHA256 失败\n");
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return 0;
    }

    // 设置 MGF1 使用 SHA256
    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
        fprintf(stderr, "设置 MGF1 SHA256 失败\n");
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return 0;
    }

    int rsa_size = RSA_size(rsa_pubkey);
    int offset = 0;
    int total_len = 0;

    // 计算需要多少段
    int num_segments = (plaintext_len + segment_size - 1) / segment_size;
    printf("需要加密 %d 段数据\n", num_segments);

    for (int i = 0; i < num_segments; i++) {
        // 计算当前段的长度
        int current_segment_len = segment_size;
        if (i == num_segments - 1) {
            // 最后一段可能不足 segment_size
            current_segment_len = plaintext_len - i * segment_size;
        }

        // 计算当前段在缓冲区中的位置
        unsigned char* current_segment = plaintext + i * segment_size;
        unsigned char* output_segment = encrypted_data + i * rsa_size;

        // 首先获取所需的输出缓冲区大小
        size_t outlen;
        if (EVP_PKEY_encrypt(ctx, NULL, &outlen, current_segment, current_segment_len) <= 0) {
            fprintf(stderr, "获取加密输出大小失败 (段 %d)\n", i);
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            return 0;
        }

        // 执行加密
        if (EVP_PKEY_encrypt(ctx, output_segment, &outlen, current_segment, current_segment_len) <= 0) {
            fprintf(stderr, "RSA 加密失败 (段 %d)\n", i);
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            return 0;
        }

        total_len += (int)outlen;
        printf("段 %d: 原始长度 %d 字节 -> 加密后长度 %zu 字节\n",
            i, current_segment_len, outlen);
    }

    *total_encrypted_len = total_len;

    // 清理
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return 1;
}

void encodedata()
{
// 初始化 OpenSSL
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    const char* public_key_file = "D:\\public_key_3072.pem";
    const char* private_key_file = "D:\\private_key_3072.pem";

    // 1. 读取 RSA-3072 密钥
    printf("读取 RSA-3072 密钥...\n");
    RSA* rsa_pubkey = read_public_key(public_key_file);
    RSA* rsa_privkey = read_private_key(private_key_file);

    if (!rsa_pubkey || !rsa_privkey) {
        fprintf(stderr, "无法读取RSA密钥\n");
        return 1;
    }

    // 检查密钥大小
    int rsa_size = RSA_size(rsa_pubkey);
    printf("RSA 密钥大小: %d 位 (%d 字节)\n", rsa_size * 8, rsa_size);

    if (rsa_size * 8 != 3072) {
        printf("警告: 密钥不是3072位，实际为 %d 位\n", rsa_size * 8);
    }

    // 3. 计算最大分段大小
    int max_segment_size = get_max_oaep_sha256_input_len(rsa_pubkey);

    // 4. 准备要加密的数据（4096字节）
    const int DATA_SIZE = 4096;
    unsigned char plaintext[DATA_SIZE];
    unsigned char decrypted_text[DATA_SIZE];

    // 填充示例数据
    printf("准备 %d 字节的测试数据...\n", DATA_SIZE);
    for (int i = 0; i < DATA_SIZE; i++) {
        plaintext[i] = i % 256;
    }

    // 5. 计算需要的缓冲区大小
    // 每段加密后的大小是 rsa_size 字节
    int num_segments = (DATA_SIZE + max_segment_size - 1) / max_segment_size;
    int encrypted_buffer_size = num_segments * rsa_size;

    printf("数据分段: %d 字节 / %d 字节每段 = %d 段\n",
        DATA_SIZE, max_segment_size, num_segments);
    printf("加密后总大小: %d 段 * %d 字节 = %d 字节\n",
        num_segments, rsa_size, encrypted_buffer_size);

    // 分配加密缓冲区
    unsigned char* encrypted_data = (unsigned char*)malloc(encrypted_buffer_size);
    if (!encrypted_data) {
        fprintf(stderr, "无法分配加密缓冲区\n");
        RSA_free(rsa_pubkey);
        RSA_free(rsa_privkey);
        return 1;
    }

    // 6. 使用 RSA-3072 公钥和 OAEP-SHA256 分段加密数据
    printf("使用 RSA-3072 公钥和 OAEP-SHA256 分段加密数据...\n");
    int total_encrypted_len = 0;

    if (!rsa_encrypt_segmented_oaep_sha256(rsa_pubkey, plaintext, DATA_SIZE,
        encrypted_data, &total_encrypted_len,
        max_segment_size)) {
        fprintf(stderr, "RSA 分段加密失败\n");
        free(encrypted_data);
        RSA_free(rsa_pubkey);
        RSA_free(rsa_privkey);
        return 1;
    }

    printf("加密后总长度: %d 字节\n", total_encrypted_len);
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
    ASSERT_EQ(result, HUKS_SUCCESS);
    std::filesystem::remove(testPath);
}

HWTEST_F(HdcHuksTest, TestUseRsaPubkeyEncode, TestSize.Level0)
{
    HdcHuks huks(TEST_HUKS_ALIAS);
    std::filesystem::path testPath = "/data/local/tmp/hdc_test_public_key.pem";
    ASSERT_FALSE(std::filesystem::exists(testPath));
    int32_t result = huks.GenerateAndExportHuksRSAPublicKey();
    ASSERT_EQ(result, HUKS_SUCCESS);
    
}

} // namespace Hdc