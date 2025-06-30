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
#include "hdc_huks.h"
namespace Hdc {
#define PASSWORD_LENGTH 10

constexpr char* hdcCredentialSocket = "/data/hdc_credential_socket";
struct CredetialMessage {
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
private:
    uint8_t pwd[PASSWORD_LENGTH];
    std::string encryptPwd;
    HdcHuks hdcHuks;
    char GetHexChar(uint8_t data);
    void ByteToHex(std::vector<uint8_t>& byteData);
    bool HexToByte(std::vector<uint8_t>& hexData);
    uint8_t HexCharToInt(uint8_t data);
    void ClearEncryptPwd(void);
};
} // namespace Hdc
#endif // HDC_PASSWORD_H