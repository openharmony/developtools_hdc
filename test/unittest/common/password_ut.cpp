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
#include "password_ut.h"
#include "securec.h"

using namespace testing::ext;
namespace Hdc {

#define TEST_KEY_ALIAS "test_password_alias"
void HdcPasswordTest::SetUpTestCase() {}
void HdcPasswordTest::TearDownTestCase() {}
void HdcPasswordTest::SetUp() {}
void HdcPasswordTest::TearDown() {}

bool HdcPasswordTest::CheckPasswordFormat(std::pair<uint8_t*, int> pwdInfo)
{
    std::string specialChar = "~!@#$%^&*()-_=+\\|[{}];:'\",<.>/?";
    if (pwdInfo.first == nullptr || pwdInfo.second != PASSWORD_LENGTH) {
        return false;
    }
    
    if (specialChar.find(pwdInfo.first[0]) == std::string::npos) {
        return false;
    }
    
    if (pwdInfo.first[1] < 'a' || pwdInfo.first[1] > 'z') {
        return false;
    }
    for (int i = 2; i < pwdInfo.second; i++) {
        if (pwdInfo.first[i] < '0' || pwdInfo.first[i] > '9') {
            return false;
        }
    }
    return true;
}

HWTEST_F(HdcPasswordTest, TestGeneratePassword, TestSize.Level0)
{
    HdcPassword pwd(TEST_KEY_ALIAS);
    pwd.GeneratePassword();
    std::pair<uint8_t*, int> pwdInfo = pwd.GetPassword();
    ASSERT_EQ(CheckPasswordFormat(pwdInfo), true);
}

HWTEST_F(HdcPasswordTest, TestGetEncryptPwdLength, TestSize.Level0)
{
    // (12 bytes nonce value  + 10 bytes pwd length  + 16 bytes tag length ) * 2 (byte to hex)
    #define ENCRYPT_LENGTH 76
    HdcPassword pwd(TEST_KEY_ALIAS);
    int len = pwd.GetEncryptPwdLength();
    ASSERT_EQ(len, ENCRYPT_LENGTH);
}

#ifdef HDC_FEATURE_SUPPORT_CREDENTIAL
HWTEST_F(HdcPasswordTest, TestEncryptAndDecrypt, TestSize.Level0)
{
    HdcPassword pwd(TEST_KEY_ALIAS);
    std::pair<uint8_t*, int> plainPwd = pwd.GetPassword();
    (void)memset_s(plainPwd.first, plainPwd.second, '1', plainPwd.second);
    ASSERT_EQ(pwd.ResetPwdKey(), true);
    ASSERT_EQ(pwd.EncryptPwd(), true);
    std::string encryptPwd = pwd.GetEncryptPassword();
    std::vector<uint8_t> encryptData(encryptPwd.data(), encryptPwd.data() + encryptPwd.length());
    ASSERT_EQ(pwd.DecryptPwd(encryptData), true);
    plainPwd = pwd.GetPassword();
    ASSERT_EQ(memcmp(plainPwd.first, "1111111111", plainPwd.second), 0);
}
#endif
} // namespace Hdc