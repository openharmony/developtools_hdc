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
#include "common.h"
#include "password.h"
#include "securec.h"
#ifdef HDC_SUPPORT_ENCRYPT_PRIVATE_KEY
#include "sys/socket.h"
#endif

namespace Hdc {
static const char* SPECIAL_CHARS = "~!@#$%^&*()-_=+\\|[{}];:'\",<.>/?";
static const uint8_t INVALID_HEX_CHAR_TO_INT_RESULT = 255;

#ifdef HDC_SUPPORT_ENCRYPT_PRIVATE_KEY
bool Hdcpassword::isNumeric(const std::string& str)
{
    if (str.empty()) {
        return false;
    }
    size_t i = 0;
    for (; i < str.size(); ++i) {
        if (!std::isdigit(str[i])) {
            return false;
        }
    }
    return true;
}
int Hdcpassword::StripLeadingZeros(const std::string &input)
{
    if (input.empty() || input == "0") {
        return 0;
    }
    size_t firstNonZero = input.find_first_not_of('0');
    if (firstNonZero == std::string::npos) {
        return 0;
    }

    std::string numberStr = input.substr(firstNonZero);
    if (!IsNumeric(numberStr)) {
        WRITE_LOG(LOG_FATAL, "stripLeadingZeros: invalid numeric string.");
        return -1;
    }
    return std::stoi(input.substr(firstNonZero));
}

bool Hdcpassword::IsInRange(int value, int min, int max)
{
    return (value >= min && value <= max);
}

CredentialMessage Hdcpassword::ParseMessage(const std::string &messageStr)
{
    if (messageStr.empty() || messageStr.length() < MESSAGE_BODY_POS) {
        WRITE_LOG(LOG_FATAL, "ParseMessage: messageStr is empty or too short.messageStr is:%s", messageStr.c_str());
        return CredetialMessage();
    }

    CredetialMessage messageStruct;
    int versionInt = messageStr[MESSAGE_VERSION_POS] - '0';
    if (!IsInRange(versionInt, METHOD_VERSION_V1, METHOD_VERSION_MAX)) {
        WRITE_LOG(LOG_FATAL, "ParseMessage: Invalid message version %d.", versionInt);
        return CredetialMessage();
    }

    messageStruct.messageVersion = versionInt;
    std::string messageMethodStr = messageStr.substr(MESSAGE_METHOD_POS, MESSAGE_METHOD_LEN);
    messageStruct.messageMethodType = StripLeadingZeros(messageMethodStr);

    std::string messageLengthStr = messageStr.substr(MESSAGE_LENGTH_POS, MESSAGE_LENGTH_LEN);
    char *end = nullptr;
    size_t bodyLength = strtol(messageLengthStr.c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || bodyLength > MESSAGE_STR_MAX_LEN) {
        WRITE_LOG(LOG_FATAL, "ParseMessage: Invalid message length %s.", messageLengthStr.c_str());
        return CredetialMessage();
    }

    if (messageStr.length() < MESSAGE_BODY_POS + bodyLength) {
        WRITE_LOG(LOG_FATAL, "ParseMessage: messageStr is too short for the body length.");
        return CredetialMessage();
    }

    messageStruct.messageBodyLen = bodyLength;
    messageStruct.messageBody = messageStr.substr(MESSAGE_BODY_POS, bodyLength);
    
    return messageStruct;
}

std::string HdcPassword::SendToUnixSocketAndRecvStr(const char *socketPath, const std::string &messageStr)
{
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to create socket.");
        return "";
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to connect to socket.");
        close(sockfd);
        return "";
    }

    ssize_t bytesSend = send(sockfd, messageStr.c_str(), messageStr.size(), 0);
    if (bytesSend < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to send message.");
        close(sockfd);
        return "";
    }

    std::string response;
    char buffer[MESSAGE_STR_MAX_LEN] = {0};
    ssize_t bytesRead = 0;
    while ((bytesRead = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        response.append(buffer, bytesRead);
        memset(buffer, 0, sizeof(buffer));
    }
    if (bytesRead < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to read from socket.");
        close(sockfd);
        return "";
    }
    
    close(sockfd);
    return response;
}

std::vector<uint8_t> HdcPassword::String2Uint8(const std::string &str, size_t len)
{
    std::vector<uint8_t> byteData(len);
    for (size_t i = 0; i < len; i++) {
        byteData[i] = static_cast<uint8_t>(str[i]);
    }
    return byteData;
}

std::string HdcPassword::ConvertInt(int len, int max_len)
{
    std::ostringstream ss;
    oss << std::setfill('0') << std::setw(max_len) << len;

    return ss.str();
}

std::string HdcPassword::SplicMessageStr(const std::string &str, const size_t type)
{
    std::ostringstream oss;
    oss << ConvertInt(METHOD_VERSION_V1, 1) // version

    std:string messageMethodStr = ConvertInt(type, MESSAGE_METHOD_LEN);
    if (messageMethodStr.length() != MESSAGE_METHOD_LEN) {
        WRITE_LOG(LOG_FATAL, "messageMethodStr length must be:%d,now is:%s", MESSAGE_METHOD_LEN, messageMethodStr.c_str());
        return "";
    }
    oss << messageMethodStr;

    std::string messageBodyLen = std::to_string(str.length());
    if (messageBodyLen.length() > MESSAGE_LENGTH_LEN) {
        WRITE_LOG(LOG_FATAL, "messageBodyLen length must be:%d,now is:%s", MESSAGE_LENGTH_LEN, messageBodyLen.c_str());
        return "";
    }
    oss << ConvertInt(str.length(), MESSAGE_LENGTH_LEN);
    oss << str;
    
    std::string messageStr = oss.str();
    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxxxxx messageStr is:%s", messageStr.c_str());
    return messageStr;
    // return oss.str();
}

std::vector<uint8_t> HdcPassword::EncryptGetPwdValue(uint8_t *pwd, int pwdLen)
{
    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxxxxx EncryptGetPwdValue, pwd is:%s, pwdLen is:%d", std::string(reinterpret_cast<char*>(pwd)).c_str(), pwdLen);
    std::string pwdStr(reinterpret_cast<const char*>(pwd), pwdLen);
    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxxxxx EncryptGetPwdValue, pwdStr is:%s", pwdStr.c_str());
    std::string sendStr = SplicMessageStr(pwdStr, METHOD_ENCRYPT);
    std::string recvStr = SendToUnixSocketAndRecvStr(CREDENTIAL_SOCKET_PATH, sendStr.c_str());
    if (recvStr.empty()) {
        WRITE_LOG(LOG_FATAL, "recvStr is empty.");
        return std::vector<uint8_t>();
    }

    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxxxxx EncryptGetPwdValue, recvStr is:%s", recvStr.c_str());
    CredetialMessage messageStruct = ParseMessage(recvStr);
    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxxxxx EncryptGetPwdValue, messageBodyLen is:%d", messageStruct.messageBodyLen);
    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxxxxx EncryptGetPwdValue, messageBody is:%s", messageStruct.messageBody.c_str());
    if (messageStruct.messageBodyLen > 0) {
        return String2Uint8(messageStruct.messageBody, messageStruct.messageBodyLen);
    } else {
        WRITE_LOG(LOG_FATAL, "Error: messageBodyLen is 0.");
        return {};
    }
}

std::pair<uint8_t*, int> HdcPassword::DecryptGetPwdValue(const std::string &encryptData)
{
    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxxxxx DecryptGetPwdValue, encryptData is:%s", encryptData.c_str());
    std::string sendStr = SplicMessageStr(encryptData, METHOD_DECRYPT);
    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxxxxx DecryptGetPwdValue, sendStr is:%s", sendStr.c_str());
    std::string recvStr = SendToUnixSocketAndRecvStr(CREDENTIAL_SOCKET_PATH, sendStr.c_str());
    WRITE_LOG
    if (recvStr.empty()) {
        WRITE_LOG(LOG_FATAL, "recvStr is empty.");
        return std::make_pair(nullptr, 0);
    }

    CredetialMessage messageStruct = ParseMessage(recvStr);
    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxxxxx DecryptGetPwdValue, messageBodyLen is:%d", messageStruct.messageBodyLen);
    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxxxxx DecryptGetPwdValue, messageBody is:%s", messageStruct.messageBody.c_str());
    if (messageStruct.messageBodyLen > 0) {
        uint8_t *pwd = new uint8_t[messageStruct.messageBodyLen];
        memcpy_s(pwd, messageStruct.messageBodyLen, messageStruct.messageBody.data(), messageStruct.messageBodyLen);
        return std::make_pair(pwd, messageStruct.messageBodyLen);
    } else {
        WRITE_LOG(LOG_FATAL, "Error: messageBodyLen is 0.");
        return std::make_pair(nullptr, 0);
    }
}
#endif
HdcPassword::HdcPassword(const std::string &pwdKeyAlias):hdcHuks(pwdKeyAlias)
{
    memset_s(pwd, sizeof(pwd), 0, sizeof(pwd));
}

HdcPassword::~HdcPassword()
{
    memset_s(pwd, sizeof(pwd), 0, sizeof(pwd));
}

std::pair<uint8_t*, int> HdcPassword::GetPassword(void)
{
    return std::make_pair(pwd, PASSWORD_LENGTH);
}

std::string HdcPassword::GetEncryptPassword(void)
{
    return encryptPwd;
}

void HdcPassword::GeneratePassword(void)
{
    // password = 1special letter + 1lower letter + 8number letter
    uint32_t randomNum = Base::GetSecureRandom();
    this->pwd[0] = SPECIAL_CHARS[(randomNum & 0xFFFF0000) % strlen(SPECIAL_CHARS)];
    const int lowercaseCount = 26;
    this->pwd[1] = 'a' + ((randomNum & 0x0000FFFF) % lowercaseCount);

    const int numberCount = 10;
    int numberStartIndex = 2;
    randomNum = Base::GetSecureRandom();
    unsigned char *randomChar = reinterpret_cast<unsigned char*>(&randomNum);
    for (int i = 0; i < static_cast<int>(sizeof(randomNum)) && numberStartIndex < PASSWORD_LENGTH; i++) {
        unsigned char byteValue = randomChar[i];
        this->pwd[numberStartIndex++] = ((byteValue >> 4) % numberCount) + '0'; // 4 high 4 bits
        this->pwd[numberStartIndex++] = ((byteValue & 0x0F) % numberCount) + '0';
    }
}

char HdcPassword::GetHexChar(uint8_t data)
{
    const uint8_t decimal = 10;
    return ((data < decimal) ? ('0' + data) : ('A' + (data - decimal)));
}

void HdcPassword::ByteToHex(std::vector<uint8_t>& byteData)
{
    uint8_t tmp;
    for (size_t i = 0; i < byteData.size(); i++) {
        tmp = byteData[i];
        encryptPwd.push_back(GetHexChar(tmp >> 4)); // 4 get high 4 bits
        encryptPwd.push_back(GetHexChar(tmp & 0x0F));
    }
}

bool HdcPassword::HexToByte(std::vector<uint8_t>& hexData)
{
    if ((hexData.size() % 2) != 0) { //2 hexData len must be even
        WRITE_LOG(LOG_FATAL, "invalid data size %d", hexData.size());
        return false;
    }
    for (size_t i = 0; i < hexData.size(); i += 2) { // 2 hex to 1 byte
        uint8_t firstHalfByte = HexCharToInt(hexData[i]);
        uint8_t lastHalfByte = HexCharToInt(hexData[i + 1]);
        if (firstHalfByte == INVALID_HEX_CHAR_TO_INT_RESULT || lastHalfByte == INVALID_HEX_CHAR_TO_INT_RESULT) {
            return false;
        }
        encryptPwd.push_back((firstHalfByte << 4) | lastHalfByte); // 4 high 4 bites
    }
    return true;
}

uint8_t HdcPassword::HexCharToInt(uint8_t data)
{
    const uint8_t decimal = 10;
    
    if (data >= '0' && data <= '9') {
        return data - '0';
    }
    if (data >= 'A' && data <= 'F') {
        return data - 'A' + decimal;
    }
    
    WRITE_LOG(LOG_FATAL, "invalid data %d", data);
    return INVALID_HEX_CHAR_TO_INT_RESULT;
}

bool HdcPassword::DecryptPwd(std::vector<uint8_t>& encryptData)
{
    bool success = false;

    ClearEncryptPwd();
    if (!HexToByte(encryptData)) {
        return false;
    }
    #ifdef HDC_SUPPORT_ENCRYPT_PRIVATE_KEY
    std::pair<uint8_t*, int> result = DecryptGetPwdValue(encryptPwd);
    #else
    std::pair<uint8_t*, int> result = hdcHuks.AesGcmDecrypt(encryptPwd);
    #endif
    if (result.first == nullptr) {
        return false;
    }
    do {
        if (result.second != PASSWORD_LENGTH) {
            WRITE_LOG(LOG_FATAL, "Invalid pwd len %d", result.second);
            break;
        }
        int ret = memcpy_s(this->pwd, PASSWORD_LENGTH, result.first, result.second);
        if (ret != EOK) {
            WRITE_LOG(LOG_FATAL, "copy pwd failed %d", ret);
            break;
        }
        success = true;
    } while (0);

    memset_s(result.first, result.second, 0, result.second);
    delete[] result.first;
    return success;
}

bool HdcPassword::EncryptPwd(void)
{
    std::vector<uint8_t> encryptData;
    ClearEncryptPwd();
    #ifdef HDC_SUPPORT_ENCRYPT_PRIVATE_KEY
    encryptData = EncryptGetPwdValue(pwd, PASSWORD_LENGTH);
    #else
    bool encryptResult = hdcHuks.AesGcmEncrypt(pwd, PASSWORD_LENGTH, encryptData);
    if (!encryptResult) {
        return false;
    }
    #endif
    ByteToHex(encryptData);
    return true;
}

bool HdcPassword::ResetPwdKey()
{
    return hdcHuks.ResetHuksKey();
}

int HdcPassword::GetEncryptPwdLength()
{
    return HdcHuks::CaculateGcmEncryptLen(PASSWORD_LENGTH) * 2; //2, bytes to hex
}

void HdcPassword::ClearEncryptPwd(void)
{
    encryptPwd.clear();
}
} // namespace Hdc