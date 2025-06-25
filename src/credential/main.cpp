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

#include "hdc_credential.h"

using namespace Hdc;

std::string convertInt(int len, int maxLen)
{
    std::ostringstream oss;
    oss << std::setw(maxLen) << std::setfill('0') << len;
    if (oss.str().length() > maxLen) {
        WRITE_LOG(LOG_FATAL, "convertInt: length exceeds max length");
        return "";
    }
    return oss.str();
}

bool isNumeric(const std::string& str)
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

int stripLeadingZeros(const std::string& input)
{
    if (input.empty() || input == "0") {
        return 0;
    }
    size_t firstNonZero = input.find_first_not_of('0');
    if (firstNonZero == std::string::npos) {
        return 0;
    }

    std::string numberStr = input.substr(firstNonZero);
    if (!isNumeric(numberStr)) {
        WRITE_LOG(LOG_FATAL, "stripLeadingZeros: invalid numeric string.");
        return -1;
    }
    return std::stoi(input.substr(firstNonZero));
}

bool isInRange(int value, int min, int max)
{
    return value >= min && value <= max;
}

Hdc::HdcHuks hdc_huks(HDC_PRIVATE_KEY_FILE_PWD_KEY_ALIAS);
Hdc::HdcPassword pwd(HDC_PRIVATE_KEY_FILE_PWD_KEY_ALIAS);

bool ResetHuksKey(void)
{
    return hdc_huks.ResetHuksKey();
}

std::string BytetoHex(const uint8_t* byteDate, size_t length)
{
    uint8_t tmp;
    std:string encrypePwd;

    for (size_t i = 0; i < length; i++) {
        tmp = byteDate[i];
        encryptPwd.push_back(pwd.GetHexChar(tmp >> 4));
        encryptPwd.push_back(pwd.GetHexChar(tmp & 0x0F));
    }
    return encryptPwd;
}

std::string CredentialEncryptPwd(const std::string& messageStr)
{
    std::vector<uint8_t> encryptData;
    const char* rawCharData = messageStr.c_str();
    const uint8_t* uint8MessageStr = reinterpret_cast<const uint8_t*>(rawCharData);

    bool encryptResult = hdc_huks.AesGcmEncrypt(uint8MessageStr, PASSWORD_LENGTH, encryptData);
    if (!encryptResult) {
        WRITE_LOG(LOG_FATAL, "CredentialEncryptPwd: AES GCM encryption failed.");
        return "";
    }

    return std::string(reinterpret_cast<const char*>(encryptData.data()), encryptData.size());
}

std::pair<std::string, size_t> EncryptPwd(const std::string& messageStr)
{
    std::cout << "EncryptPwd: messageStr = " << messageStr << std::endl;

    if (!ResetPwdKey()) {
        WRITE_LOG(LOG_FATAL, "EncryptPwd: ResetPwdKey failed.");
        return std::make_pair(std::string(), 0);
    }

    std::string encryptPwd = CredentialEncryptPwd(messageStr);
    if (encryptPwd.empty()) {
        WRITE_LOG(LOG_FATAL, "EncryptPwd: CredentialEncryptPwd failed.");
        return std::make_pair(std::string(), 0);
    }

    return std::make_pair(encryptPwd, encryptPwd.size());
}

std::pair<std::string, size_t> DecryptPwd(const std::string& messageStr)
{
    std::cout << "DecryptPwd: encryptPwd = " << encryptPwd << std::endl;

    uint8_t pwd[PASSWORD_LENGTH] = {0};
    std::pair<std::string, size_t> decryptPwd = hdc_huks.AesGcmDecrypt(messageStr);
    if (decryptPwd.first.empty()) {
        WRITE_LOG(LOG_FATAL, "DecryptPwd: AesGcmDecrypt failed.");
        return std::make_pair(std::string(), 0);
    }

    do {
        if (decryptPwd.second != PASSWORD_LENGTH) {
            WRITE_LOG(LOG_FATAL, "DecryptPwd: decryptPwd.second != PASSWORD_LENGTH.");
            break;
        }
        int ret = memcpy_s(pwd, PASSWORD_LENGTH, decryptPwd.first, decryptPwd.second);
        if (ret != EOK) {
            WRITE_LOG(LOG_FATAL, "DecryptPwd: memcpy_s failed.");
            break;
        }
    } while (0);

    memset_s(decryptPwd.first, decryptPwd.second, 0, decryptPwd.second);
    delete[] decryptPwd.first;

    std::string pwd_str(reinterpret_cast<const char*>(pwd), PASSWORD_LENGTH);
    return std::make_pair(pwd_str, pwd_str.size());
}

CredentailMessage ParseMessage(const std::string& messageStr)
{
    if (messageStr.empty() || messageStr.length() < MESSAGE_BODY_POS) {
        std::cerr << "ParseMessage: messageStr is empty or too short.messageStr is:" << messageStr.c_str() << std::endl;
        WRITE_LOG(LOG_FATAL, "ParseMessage: messageStr is empty or too short..messageStr is:%s", messageStr.c_str());
        return CredentailMessage();
    }

    CredentailMessage messageStruct;
    int versionInt = messageStr[MESSAGE_VERSION_POS] - '0';
    if (!isInRange(versionInt, METHOD_VERSION_V1, METHOD_VERSION_MAX)) {
        std::cerr << "ParseMessage: Invalid message version is: " << versionInt << std::endl;
        WRITE_LOG(LOG_FATAL, "ParseMessage: Invalid message version %d.", versionInt);
        return CredentailMessage();
    }

    messageStruct.messageVersion = versionInt;
    std::string messageMethodStr = messageStr.substr(MESSAGE_METHOD_POS, MESSAGE_METHOD_LEN);
    messageStruct.messageMethodType = stripLeadingZeros(messageMethodStr);

    std::string messageLengthStr = messageStr.substr(MESSAGE_LENGTH_POS, MESSAGE_LENGTH_LEN);
    char *end = nullptr;
    size_t bodyLength = strtol(messageLengthStr.c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || bodyLength > MESSAGE_STR_MAX_LEN) {
        std::cerr << "ParseMessage: Invalid message length is: " << messageLengthStr << std::endl;
        WRITE_LOG(LOG_FATAL, "ParseMessage: Invalid message length %s.", messageLengthStr.c_str());
        return CredentailMessage();
    }

    if (messageStr.length() < MESSAGE_BODY_POS + bodyLength) {
        std::cerr << "ParseMessage: messageStr is too short for the body length." << std::endl;
        WRITE_LOG(LOG_FATAL, "ParseMessage: messageStr is too short for the body length.");
        return CredentailMessage();
    }

    messageStruct.messageBodyLen = static_cast<int>(bodyLength);
    messageStruct.messageBody = messageStr.substr(MESSAGE_BODY_POS, bodyLength);

    return messageStruct;
}

std::string ConstructMessage(const CredentailMessage& messageStruct)
{
    std::ostringstream messageStream; 

    std::cout << "ConstructMessage: messageStruct.messageVersion = " << messageStruct.messageVersion << std::endl;
    std::cout << "ConstructMessage: messageStruct.messageMethodType = " << messageStruct.messageMethodType << std::endl;
    std::cout << "ConstructMessage: messageStruct.messageBodyLen = " << messageStruct.messageBodyLen << std::endl;
    std::cout << "ConstructMessage: messageStruct.messageBody = " << messageStruct.messageBody << std::endl;

    messageStream << messageStruct.messageVersion;

    std::string messageMethodTypeStr = convertInt(messageStruct.messageMethodType, MESSAGE_METHOD_LEN);
    if (messageMethodTypeStr.size() != MESSAGE_METHOD_LEN) {
        std::cerr << "ConstructMessage: messageMethodTypeStr size is not equal to " << MESSAGE_METHOD_LEN << std::endl;
        WRITE_LOG(LOG_FATAL, "ConstructMessage: messageMethodTypeStr size is not equal to %d.", MESSAGE_METHOD_LEN);
        return "";
    }
    messageStream << messageMethodTypeStr;

    std::string messageLengthStr = std::to_string(messageStruct.messageBodyLen);
    if (messageLengthStr.size() > MESSAGE_LENGTH_LEN) {
        std::cerr << "ConstructMessage: messageLengthStr size exceeds " << MESSAGE_LENGTH_LEN << std::endl;
        WRITE_LOG(LOG_FATAL, "ConstructMessage: messageLengthStr size exceeds %d.", MESSAGE_LENGTH_LEN);
        return "";
    }
    messageStream << convertInt(messageLengthStr.size(), MESSAGE_LENGTH_LEN);
    messageStream << messageStruct.messageBody;

    std::cout << "ConstructMessage: Final message = " << messageStream.str() << std::endl;
    return messageStream.str();
}

/*解析字符串，加密，再返回新的字符串*/
std::string ParseAndProcessMessageStr(const std::string& messageStr)
{
    CredetialMessage messageStruct = ParseMessage(messageStr);
    if (messageStruct.messageBody.empty() || messageStruct.messageVersion != METHOD_VERSION_V1) {
        std::cerr << "Error: Invalid message structure.And Version is:" << messageStruct.messageVersion << std::endl;
        WRITE_LOG(LOG_FATAL, "Error: Invalid message structure. and Version is: %d", messageStruct.messageVersion);
        return "";
    }
    std::pair<std::string, size_t> processMessageValue;
    switch (messageStruct.messageMethodType) {
        case METHOD_ENCRYPT: {
            processMessageValue = EncryptPwd(messageStruct.messageBody);
            break;
        }
        case METHOD_DECRYPT: {
            processMessageValue = DecryptPwd(messageStruct.messageBody);
            break;
        }
        default:
            break;
    }

    messageStruct.messageBody = processMessageValue.first;
    messageStruct.messageBodyLen = processMessageValue.second;

    return ConstructMessage(messageStruct);
}

int create_and_bind_socket(const std::string& socketPath)
{
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "create_and_bind_socket: Failed to create socket." << std::endl;
        WRITE_LOG(LOG_FATAL, "create_and_bind_socket: Failed to create socket.");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "create_and_bind_socket: Failed to set socket options." << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        WRITE_LOG(LOG_FATAL, "Error: Failed to set socket options, message: %s.", strerror(errno));
        close(sockfd);
        return -1;
    }
    unlink(socketPath.c_str()); // Remove the socket file if it already exists

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "create_and_bind_socket: Failed to bind socket." << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        WRITE_LOG(LOG_FATAL, "Error: Failed to bind socket, message: %s.", strerror(errno));
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int main(void)
{
    int sockfd = create_and_bind_socket(CREDENTIAL_SOCKET_PATH);
    if (listen(sockfd, SOCKET_CLIENT_NUMS) < 0) {
        std::cerr << "main: Failed to listen on socket." << std::endl;
        WRITE_LOG(LOG_FATAL, "Error: Failed to listen on socket.");
        close(sockfd);
        return -1;
    }

    std::cout << "main: Listening on socket: " << CREDENTIAL_SOCKET_PATH << std::endl;
    WRITE_LOG(LOG_INFO, "Listening on socket: %s", CREDENTIAL_SOCKET_PATH.c_str());

    while (true) { 
        int connfd = accept(sockfd, nullptr, nullptr);
        if (connfd < 0) {
            std::cerr << "main: Failed to accept connection." << std::endl;
            WRITE_LOG(LOG_FATAL, "Error: Failed to accept connection.");
            continue;
        }

        char buffer[MESSAGE_STR_MAX_LEN] = {0};
        ssize_t bytesRead = read(connfd, buffer, sizeof(buffer) - 1);
        if (bytesRead < 0) {
            std::cerr << "main: Failed to read from socket." << std::endl;
            WRITE_LOG(LOG_FATAL, "Error: Failed to read from socket.");
            close(connfd);
            continue;
        }
        std::cout << "Received message: " << buffer << std::endl;
        std::string sendBuf = ParseAndProcessMessageStr(std::string(buffer, bytesRead));
        if (sendBuf.empty()) {
            std::cerr << "main: Processed message is empty." << std::endl;
            WRITE_LOG(LOG_FATAL, "Error: Processed message is empty.");
            close(connfd);
            continue;
        }
        size_t bytesSend = write(connfd, sendBuf.c_str(), sendBuf.size());
        if (bytesSend != sendBuf.size()) {
            std::cerr << "main: Failed to send message." << std::endl;
            WRITE_LOG(LOG_FATAL, "Error: Failed to send message.");
            close(connfd);
        }
        memset_s(buffer, sizeof(buffer), 0, sizeof(buffer)); // Clear the buffer
        close(connfd);

    } // Keep the server running indefinitely
    close(listenfd);
    unlink(SERVER_SOCKET_PATH);
    return 0;
}