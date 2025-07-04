/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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

constexpr size_t SOCKET_CLIENT_NUMS = 1;

using namespace Hdc;
using namespace OHOS;

ReminderEventManager::ReminderEventSubscriber::ReminderEventSubscriber(
    const EventFwk::CommonEventSubscribeInfo &subscriberInfo)
    : CommonEventSubscriber(subscriberInfo)
{
}

void ReminderEventManager::ReminderEventSubscriber::OnReceiveEvent(
    const EventFwk::CommonEventData &data)
{
    AAFwk::Want want = data.GetWant();
    std::string action = want.GetAction();
    std::string accountUserId = std::to_string(data.GetCode());
    std::string path = std::string("/data/service/el1/public/hdc_server/") +
        accountUserId.c_str();
    mode_t mode = (S_IRWXU | S_IRWXG);

    if (action == EventFwk::CommonEventSupport::COMMON_EVENT_USER_ADDED) {
        if (::mkdir(path.c_str(), mode) != 0) {
            WRITE_LOG(LOG_FATAL, "Failed to create directory ,error is :%s", strerror(errno));
            return;
        }
        if (::chmod(path.c_str(), mode) != 0) {
            WRITE_LOG(LOG_FATAL, "Failed to set directory permissions, error is :%s", strerror(errno));
            return;
        }
    }
    if (action == EventFwk::CommonEventSupport::COMMON_EVENT_USER_REMOVED) {
        Base::RemovePath(path.c_str());
    }
    return;
}

ReminderEventManager::ReminderEventManager()
{
    Init();
}

void ReminderEventManager::Init()
{
    EventFwk::MatchingSkills customMatchingSkills; 
    customMatchingSkills.AddEvent(EventFwk::COMMON_EVENT_USER_ADDED);
    customMatchingSkills.AddEvent(EventFwk::COMMON_EVENT_USER_REMOVED);

    EventFwk::CommonEventSubscribeInfo customSubscriberInfo(customMatchingSkills);
    customSubscriberInfo.SetThreadMode(EventFwk::CommonEventSubscribeInfo::COMMON);
    customSubscriber = std::make_shared<ReminderEventSubscriber>(customSubscriberInfo);

    const int MAX_RETRY = 10;
    int retryCount = 0;

    while (retryCount < MAX_RETRY &&
        !EventFwk::CommonEventManager::SubscribeCommonEvent(customSubscriber)) {
        ++retryCount;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        WRITE_LOG(LOG_DEBUG, "SubscribeCommonEvent fail %d/%d", retryCount, MAX_RETRY);
    }

    if (retryCount < MAX_RETRY) {
        WRITE_LOG(LOG_ERROR, "SubscribeCommonEvent success.");
    } else {
        WRITE_LOG(LOG_DEBUG, "SubscribeCommonEvent failed after %d retries.", MAX_RETRY);
    }
    return;
}
std::string convertInt(int len, int maxLen)
{
    std::ostringstream oss;
    oss << std::setw(maxLen) << std::setfill('0') << len;
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

bool ResetPwdKey(void)
{
    return hdc_huks.ResetHuksKey();
}

std::string BytetoHex(const uint8_t* byteDate, size_t length)
{
    uint8_t tmp;
    std::string encryptPwd;

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
    WRITE_LOG(LOG_DEBUG, "xxxxxxxxxxxxxxxxxxxxxxxx EncryptPwd: messageStr = %s", messageStr.c_str());

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
    WRITE_LOG(LOG_DEBUG, "xxxxxxxxxxxxxxxxxxxxxxxx DecryptPwd: messageStr = %s", messageStr.c_str());

    uint8_t pwd[PASSWORD_LENGTH] = {0};
    std::pair<uint8_t*, int> decryptPwd = hdc_huks.AesGcmDecrypt(messageStr);
    if (decryptPwd.first == nullptr) {
        WRITE_LOG(LOG_FATAL, "AesGcmDecrypt failed.");
        return std::make_pair(std::string(), 0);
    }

    do {
        if (decryptPwd.second != PASSWORD_LENGTH) {
            WRITE_LOG(LOG_FATAL, "Invalid pwd len %d", decryptPwd.second);
            break;
        }
        int ret = memcpy_s(pwd, PASSWORD_LENGTH, decryptPwd.first, decryptPwd.second);
        if (ret != EOK) {
            WRITE_LOG(LOG_FATAL, "Copy failed.ret is %d", ret);
            break;
        }
    } while (0);

    memset_s(decryptPwd.first, decryptPwd.second, 0, decryptPwd.second);
    delete[] decryptPwd.first;

    std::string pwd_str(reinterpret_cast<const char*>(pwd), PASSWORD_LENGTH);

    return std::make_pair(pwd_str, pwd_str.size());
}

CredentialMessage ParseMessage(const std::string& messageStr)
{
    if (messageStr.empty() || messageStr.length() < MESSAGE_BODY_POS) {
        WRITE_LOG(LOG_FATAL, "messageStr is empty or too short.messageStr is:%s", messageStr.c_str());
        return CredentialMessage();
    }

    int versionInt = messageStr[MESSAGE_VERSION_POS] - '0';
    if (!isInRange(versionInt, METHOD_VERSION_V1, METHOD_VERSION_MAX)) {
        WRITE_LOG(LOG_FATAL, "Invalid message version %d.", versionInt);
        return CredentialMessage();
    }

    CredentialMessage messageStruct;
    messageStruct.messageVersion = versionInt;
    std::string messageMethodStr = messageStr.substr(MESSAGE_METHOD_POS, MESSAGE_METHOD_LEN);
    messageStruct.messageMethodType = stripLeadingZeros(messageMethodStr);

    std::string messageLengthStr = messageStr.substr(MESSAGE_LENGTH_POS, MESSAGE_LENGTH_LEN);
    char *end = nullptr;
    size_t bodyLength = strtol(messageLengthStr.c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || bodyLength > MESSAGE_STR_MAX_LEN) {
        WRITE_LOG(LOG_FATAL, "Invalid message length.");
        return CredentialMessage();
    }

    if (messageStr.length() < MESSAGE_BODY_POS + bodyLength) {
        WRITE_LOG(LOG_FATAL, "messageStr is too short.messageStr is:%s", messageStr.c_str());
        return CredentialMessage();
    }

    messageStruct.messageBodyLen = static_cast<int>(bodyLength);
    messageStruct.messageBody = messageStr.substr(MESSAGE_BODY_POS, bodyLength);

    return messageStruct;
}

std::string ConstructMessage(const CredentialMessage& messageStruct)
{
    std::ostringstream messageStream; 

    WRITE_LOG(LOG_DEBUG, "xxxxxxxxxxxxxxxxxxxxx messageStruct.messageVersion = %d", messageStruct.messageVersion);
    WRITE_LOG(LOG_DEBUG, "xxxxxxxxxxxxxxxxxxxxx messageStruct.messageMethodType = %d", messageStruct.messageMethodType);
    WRITE_LOG(LOG_DEBUG, "xxxxxxxxxxxxxxxxxxxxx messageStruct.messageBodyLen = %d", messageStruct.messageBodyLen);
    WRITE_LOG(LOG_DEBUG, "xxxxxxxxxxxxxxxxxxxxx messageStruct.messageBody = %s", messageStruct.messageBody.c_str());

    messageStream << messageStruct.messageVersion;

    std::string messageMethodTypeStr = convertInt(messageStruct.messageMethodType, MESSAGE_METHOD_LEN);
    if (messageMethodTypeStr.size() != MESSAGE_METHOD_LEN) {
        WRITE_LOG(LOG_FATAL, "messageMethod length Error!");
        return "";
    }
    messageStream << messageMethodTypeStr;

    std::string messageLengthStr = std::to_string(messageStruct.messageBodyLen);
    if (messageLengthStr.length() > MESSAGE_LENGTH_LEN) {
        WRITE_LOG(LOG_FATAL, "messageLength must be:%d,now is:%d",
            MESSAGE_LENGTH_LEN, messageLengthStr.length());
        return "";
    }
    messageStream << convertInt(messageStruct.messageBodyLen, MESSAGE_LENGTH_LEN);
    messageStream << messageStruct.messageBody;

    WRITE_LOG(LOG_DEBUG, "xxxxxxxxxxxxxxxxxxxxx Final message = %s", messageStream.str().c_str());
    return messageStream.str();
}

/*解析字符串，加密，再返回新的字符串*/
std::string ParseAndProcessMessageStr(const std::string& messageStr)
{
    CredentialMessage messageStruct = ParseMessage(messageStr);
    if (messageStruct.messageBody.empty() || messageStruct.messageVersion != METHOD_VERSION_V1) {
        WRITE_LOG(LOG_FATAL, "Invalid message structure or version not v1.");
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
        WRITE_LOG(LOG_FATAL, "Failed to create socket.");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to set socket options, message: %s.", strerror(errno));
        close(sockfd);
        return -1;
    }

    unlink(socketPath.c_str()); // Remove the socket file if it already exists

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to bind socket, message: %s.", strerror(errno));
        close(sockfd);
        return -1;
    }

    if (chmod(addr.sun_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) != 0) {
        WRITE_LOG(LOG_FATAL, "Failed to chmod socket file, message: %s.", strerror(errno));
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int main(void)
{
    ReminderEventManager reminderEventManager;
    int sockfd = create_and_bind_socket(hdcCredentialSocketRealPath);
    if (listen(sockfd, SOCKET_CLIENT_NUMS) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to listen on socket.");
        close(sockfd);
        return -1;
    }

    WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxx Listening on socket: %s", hdcCredentialSocketRealPath);

    while (true) { 
        int connfd = accept(sockfd, nullptr, nullptr);
        if (connfd < 0) {
            WRITE_LOG(LOG_FATAL, "Failed to accept connection!");
            continue;
        }

        char buffer[MESSAGE_STR_MAX_LEN] = {0};
        ssize_t bytesRead = read(connfd, buffer, sizeof(buffer) - 1);
        if (bytesRead < 0) {
            WRITE_LOG(LOG_FATAL, "Error: Failed to read from socket.");
            close(connfd);
            continue;
        }
        WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxx Received message: %s", buffer);
        std::string sendBuf = ParseAndProcessMessageStr(std::string(buffer, bytesRead));
        if (sendBuf.empty()) {
            WRITE_LOG(LOG_FATAL, "Error: Processed message is empty.");
            close(connfd);
            continue;
        }
        WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxx Send message: %s", sendBuf.c_str());
        WRITE_LOG(LOG_INFO, "xxxxxxxxxxxxxxxxxxxxx Send message length: %zu", sendBuf.size());

        size_t bytesSend = write(connfd, sendBuf.c_str(), sendBuf.size());
        if (bytesSend != sendBuf.size()) {
            WRITE_LOG(LOG_FATAL, "Failed to send message.");
            close(connfd);
        }
        memset_s(buffer, sizeof(buffer), 0, sizeof(buffer)); // Clear the buffer
        close(connfd);

    } // Keep the server running indefinitely
    close(sockfd);
    unlink(hdcCredentialSocketRealPath);
    return 0;
}