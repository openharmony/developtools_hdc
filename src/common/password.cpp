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
#include "sys/socket.h"

namespace Hdc {
static const char* SPECIAL_CHARS = "~!@#$%^&*()-_=+\\|[{}];:'\",<.>/?";
static const uint8_t INVALID_HEX_CHAR_TO_INT_RESULT = 255;

ssize_t HdcPassword::GetCredential(const char *socketPath, const std::string &messageStr, char data[], ssize_t size)
{
    if (data == nullptr || size < static_cast<ssize_t>(MESSAGE_STR_MAX_LEN)) {
        WRITE_LOG(LOG_FATAL, "data is null or size:%d out of range", size);
        return -1;
    }

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to create socket.");
        return -1;
    }

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    size_t maxPathLen = sizeof(addr.sun_path) - 1;
    size_t pathLen = strlen(socketPath);
    if (pathLen > maxPathLen) {
        WRITE_LOG(LOG_FATAL, "Socket path too long.");
        close(sockfd);
        return -1;
    }
    if (memcpy_s(addr.sun_path, maxPathLen, socketPath, pathLen) != EOK) {
        WRITE_LOG(LOG_FATAL, "Memcpy_s error.");
        close(sockfd);
        return -1;
    }
 
    if (connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to connect to socket.");
        close(sockfd);
        return -1;
    }

    ssize_t bytesSend = send(sockfd, messageStr.c_str(), messageStr.size(), 0);
    if (bytesSend < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to send message.");
        close(sockfd);
        return -1;
    }

    ssize_t count = 0;
    ssize_t bytesRead = 0;
    while ((bytesRead = recv(sockfd, data + count, size - 1 - count, 0)) > 0) {
        count += bytesRead;
        if (count >= size - 1) {
            WRITE_LOG(LOG_WARN, "Data truncated, buffer size limit reached.");
            break;
        }
    }
    data[count] = '\0'; // Null-terminate the received data
    if (bytesRead < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to read from socket.");
        close(sockfd);
        return -1;
    }
    
    close(sockfd);
    return count;
}

std::string HdcPassword::SplicMessageStr(const std::string &str, const size_t type)
{
    if (str.empty()) {
        WRITE_LOG(LOG_FATAL, "Input string is empty.");
        return "";
    }
    const size_t bodyLen = str.size();
    size_t totalLength = MESSAGE_METHOD_POS + MESSAGE_METHOD_LEN +
                         MESSAGE_LENGTH_LEN + bodyLen;

    std::string messageMethodTypeStr = IntToStringWithPadding(type, MESSAGE_METHOD_LEN);
    if (messageMethodTypeStr.length() != MESSAGE_METHOD_LEN) {
        WRITE_LOG(LOG_FATAL, "messageMethodTypeStr length must be:%d,now is:%s",
            MESSAGE_METHOD_LEN, messageMethodTypeStr.c_str());
        return "";
    }

    std::string messageBodyLen = IntToStringWithPadding(str.length(), MESSAGE_LENGTH_LEN);
    if (messageBodyLen.empty() || (messageBodyLen.length() > MESSAGE_LENGTH_LEN)) {
        WRITE_LOG(LOG_FATAL, "messageBodyLen length must be:%d,now is:%s", MESSAGE_LENGTH_LEN, messageBodyLen.c_str());
        return "";
    }

    std::string result;
    result.reserve(totalLength);
    result.push_back('0' + METHOD_VERSION_V1);
    result.append(messageMethodTypeStr);
    result.append(messageBodyLen);
    result.append(str);
    if (result.size() != totalLength) {
        WRITE_LOG(LOG_FATAL, "size mismatch. Expected: %zu, Actual: %zu", totalLength, result.size());
        return "";
    }
    return result;
}

std::vector<uint8_t> HdcPassword::EncryptGetPwdValue(uint8_t *pwd)
{
    std::string sendStr = SplicMessageStr(reinterpret_cast<const char*>(pwd), METHOD_ENCRYPT);
    if (sendStr.empty()) {
        WRITE_LOG(LOG_FATAL, "sendStr is empty.");
        return std::vector<uint8_t>();
    }
    char data[MESSAGE_STR_MAX_LEN] = {0};
    ssize_t count = GetCredential(HDC_CREDENTIAL_SOCKET_SANDBOX_PATH.c_str(), sendStr, data, MESSAGE_STR_MAX_LEN);
    memset_s(sendStr.data(), sendStr.size(), 0, sendStr.size());
    if (count <= 0) {
        WRITE_LOG(LOG_FATAL, "recv data is empty.");
        return std::vector<uint8_t>();
    }
    std::string recvStr(data, count);
    CredentialMessage messageStruct(recvStr);
    memset_s(recvStr.data(), recvStr.size(), 0, recvStr.size());
    memset_s(data, sizeof(data), 0, sizeof(data));
    if (messageStruct.GetMessageBodyLen() > 0) {
        std::string body = messageStruct.GetMessageBody();
        std::vector<uint8_t> retByteData = String2Uint8(body, messageStruct.GetMessageBodyLen());
        if (!body.empty()) {
            memset_s(&body[0], body.size(), 0, body.size());
        }
        return retByteData;
    } else {
        WRITE_LOG(LOG_FATAL, "Error: messageBodyLen is 0.");
        return std::vector<uint8_t>();
    }
}

std::pair<uint8_t*, int> HdcPassword::DecryptGetPwdValue(const std::string &encryptData)
{
    std::string sendStr = SplicMessageStr(encryptData, METHOD_DECRYPT);
    if (sendStr.empty()) {
        WRITE_LOG(LOG_FATAL, "sendStr is empty.");
        return std::make_pair(nullptr, 0);
    }
    char data[MESSAGE_STR_MAX_LEN] = {0};
    ssize_t count = GetCredential(HDC_CREDENTIAL_SOCKET_SANDBOX_PATH.c_str(), sendStr, data, MESSAGE_STR_MAX_LEN);
    memset_s(sendStr.data(), sendStr.size(), 0, sendStr.size());
    if (count <= 0) {
        WRITE_LOG(LOG_FATAL, "recv data is empty.");
        return std::make_pair(nullptr, 0);
    }
    std::string recvStr(data, count);
    CredentialMessage messageStruct(recvStr);
    memset_s(recvStr.data(), recvStr.size(), 0, recvStr.size());
    memset_s(data, sizeof(data), 0, sizeof(data));
    if (messageStruct.GetMessageBodyLen() > 0) {
        uint8_t *keyData = new uint8_t[messageStruct.GetMessageBodyLen() + 1];
        std::copy(messageStruct.GetMessageBody().begin(), messageStruct.GetMessageBody().end(), keyData);
        keyData[messageStruct.GetMessageBodyLen()] = '\0';
        return std::make_pair(keyData, messageStruct.GetMessageBodyLen());
    } else {
        WRITE_LOG(LOG_FATAL, "Error: messageBodyLen is 0.");
        return std::make_pair(nullptr, 0);
    }
}
HdcPassword::HdcPassword(const std::string &pwdKeyAlias):hdcHuks(pwdKeyAlias)
{
    (void)memset_s(pwd, sizeof(pwd), 0, sizeof(pwd));
}

HdcPassword::~HdcPassword()
{
    (void)memset_s(pwd, sizeof(pwd), 0, sizeof(pwd));
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
    if ((hexData.size() % 2) != 0) { // 2 hexData len must be even
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
    std::pair<uint8_t*, int> result = DecryptGetPwdValue(encryptPwd);
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

    if (memset_s(result.first, result.second, 0, PASSWORD_LENGTH) != EOK) {
        WRITE_LOG(LOG_FATAL, "memset_s failed.");
        success = false;
    }
    delete[] result.first;
    return success;
}

bool HdcPassword::EncryptPwd(void)
{
    ClearEncryptPwd();
    std::vector<uint8_t> encryptData = EncryptGetPwdValue(pwd);
    if (encryptData.size() == 0) {
        return false;
    }
    ByteToHex(encryptData);
    return true;
}

bool HdcPassword::ResetPwdKey()
{
    return hdcHuks.ResetHuksKey();
}

int HdcPassword::GetEncryptPwdLength()
{
    return HdcHuks::CaculateGcmEncryptLen(PASSWORD_LENGTH) * 2; // 2, bytes to hex
}

void HdcPassword::ClearEncryptPwd(void)
{
    encryptPwd.clear();
}
} // namespace Hdc
