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