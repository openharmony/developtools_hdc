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
#ifndef HDC_PASSWORD_H
#define HDC_PASSWORD_H
#include <vector>
#include <string>
#include <memory>
#include <utility>
#ifdef HDC_SUPPORT_ENCRYPT_PRIVATE_KEY
#include <iomanip>
#endif
#include "hdc_huks.h"
namespace Hdc {
#define PASSWORD_LENGTH 10

#ifdef HDC_SUPPORT_ENCRYPT_PRIVATE_KEY
constexpr const char* hdcCredentialSocketSandboxPath = "/data/hdc/hdc_huks/hdc_credential.socket";
constexpr const char* hdcCredentialSocketRealPath = "/data/service/el1/public/hdc_server/hdc_common/hdc_credential.socket";
struct CredentialMessage {
    int messageVersion;
    int messageMethodType;
    int messageBodyLen;
    std::string messageBody;
};
constexpr size_t MESSAGE_STR_MAX_LEN = 1024;
constexpr size_t MESSAGE_VERSION_POS = 0;
constexpr size_t MESSAGE_METHOD_POS = 1;
constexpr size_t MESSAGE_METHOD_LEN = 3;
constexpr size_t MESSAGE_LENGTH_POS = 4;
constexpr size_t MESSAGE_LENGTH_LEN = 4;
constexpr size_t MESSAGE_BODY_POS = 8;

enum V1MethodID {
    METHOD_ENCRYPT = 1,
    METHOD_DECRYPT,
};

enum MethodVersion {
    METHOD_VERSION_V1 = 1,
    METHOD_VERSION_MAX = 9,
};
#endif
class HdcPassword {
public:
    ~HdcPassword();
    explicit HdcPassword(const std::string &pwdKeyAlias);
    void GeneratePassword(void);
    bool DecryptPwd(std::vector<uint8_t>& encryptData);
    bool EncryptPwd(void);
    std::pair<uint8_t*, int> GetPassword(void);
    std::string GetEncryptPassword(void);
    bool ResetPwdKey();
    int GetEncryptPwdLength();
    char GetHexChar(uint8_t data);
#ifdef HDC_SUPPORT_ENCRYPT_PRIVATE_KEY
    bool IsNumeric(const std::string& str);
    int StripLeadingZeros(const std::string &input);
    bool IsInRange(int value, int min, int max);
    CredentialMessage ParseMessage(const std::string &messageStr);
#endif
private:
    uint8_t pwd[PASSWORD_LENGTH];
    std::string encryptPwd;
    HdcHuks hdcHuks;
    void ByteToHex(std::vector<uint8_t>& byteData);
    bool HexToByte(std::vector<uint8_t>& hexData);
    uint8_t HexCharToInt(uint8_t data);
    void ClearEncryptPwd(void);
#ifdef HDC_SUPPORT_ENCRYPT_PRIVATE_KEY
    std::string ConvertInt(int len, int maxLen);
    std::string SplicMessageStr(const std::string &str, const size_t type);
    std::vector<uint8_t> String2Uint8(const std::string &str, size_t len);
    std::string SendToUnixSocketAndRecvStr(const char *socketPath, const std::string &messageStr);
    std::vector<uint8_t> EncryptGetPwdValue(uint8_t *pwd, int pwdLen);
    std::pair<uint8_t*, int> DecryptGetPwdValue(const std::string &encryptData);
#endif
};
} // namespace Hdc
#endif // HDC_PASSWORD_H