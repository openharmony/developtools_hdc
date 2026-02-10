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
#include "credential_message.h"
#include <charconv>
#include "sys/socket.h"
#include "base.h"

using namespace Hdc;

const std::string HDC_CREDENTIAL_SOCKET_SANDBOX_PATH = "/data/hdc/hdc_huks/hdc_credential.socket";

CredentialMessage::CredentialMessage(const std::string& messageStr)
{
    Init(messageStr);
}

void CredentialMessage::Init(const std::string& messageStr)
{
    if (messageStr.empty() || messageStr.length() < MESSAGE_BODY_POS) {
        WRITE_LOG(LOG_FATAL, "messageStr is too short!");
        return;
    }

    int versionInt = messageStr[MESSAGE_VERSION_POS] - '0';
    if (versionInt < METHOD_VERSION_V1 || versionInt > METHOD_VERSION_MAX) {
        WRITE_LOG(LOG_FATAL, "Invalid message version %d.", versionInt);
        return;
    }

    messageVersion = versionInt;

    std::string messageMethodStr = messageStr.substr(MESSAGE_METHOD_POS, MESSAGE_METHOD_LEN);
    messageMethodType = StripLeadingZeros(messageMethodStr);

    std::string messageLengthStr = messageStr.substr(MESSAGE_LENGTH_POS, MESSAGE_LENGTH_LEN);
    size_t bodyLength = 0;
    auto [ptr, ec] = std::from_chars(messageLengthStr.data(),
                                     messageLengthStr.data() + messageLengthStr.size(), bodyLength);
    if (ec != std::errc()) {
        bodyLength = 0;
    }
    if (bodyLength == 0 || bodyLength > MESSAGE_STR_MAX_LEN) {
        WRITE_LOG(LOG_FATAL, "Invalid message body length %s.", messageLengthStr.c_str());
        return;
    }

    if (messageStr.length() < MESSAGE_BODY_POS + bodyLength) {
        WRITE_LOG(LOG_FATAL, "messageStr is too short.");
        return;
    }

    messageBodyLen = static_cast<int>(bodyLength);
    messageBody = messageStr.substr(MESSAGE_BODY_POS, bodyLength);
}
CredentialMessage::~CredentialMessage()
{
    if (!messageBody.empty()) {
        memset_s(&messageBody[0], messageBody.size(), 0, messageBody.size());
    }
}

void CredentialMessage::SetMessageVersion(int version)
{
    if (version >= METHOD_VERSION_V1 && version <= METHOD_VERSION_MAX) {
        messageVersion = version;
    } else {
        WRITE_LOG(LOG_FATAL, "Invalid message version %d.", version);
    }
}

void CredentialMessage::SetMessageBody(const std::string& body)
{
    if (body.size() > MESSAGE_STR_MAX_LEN) {
        WRITE_LOG(LOG_FATAL, "Message body length exceeds maximum allowed length.");
        return;
    }
    messageBody = body;
    messageBodyLen = static_cast<int>(messageBody.size());
}

std::string CredentialMessage::Construct() const
{
    size_t totalLength = 0;
    totalLength += 1;
    totalLength += MESSAGE_METHOD_LEN;
    totalLength += MESSAGE_LENGTH_LEN;
    totalLength += messageBody.size();

    std::string messageMethodTypeStr = IntToStringWithPadding(messageMethodType, MESSAGE_METHOD_LEN);
    if (messageMethodTypeStr.size() != MESSAGE_METHOD_LEN) {
        WRITE_LOG(LOG_FATAL, "messageMethod length Error!");
        return "";
    }

    std::string messageBodyLenStr = IntToStringWithPadding(messageBodyLen, MESSAGE_LENGTH_LEN);
    if (messageBodyLenStr.empty() || (messageBodyLenStr.size() > MESSAGE_LENGTH_LEN)) {
        WRITE_LOG(LOG_FATAL, "messageBodyLen length must be:%d,now is:%s",
            MESSAGE_LENGTH_LEN, messageBodyLenStr.c_str());
        return "";
    }
    
    std::string result;
    result.reserve(totalLength);
    result.push_back('0' + messageVersion);
    result.append(messageMethodTypeStr);
    result.append(messageBodyLenStr);
    result.append(messageBody);

    if (result.size() != totalLength) {
        WRITE_LOG(LOG_FATAL, "size mismatch. Expected: %zu, Actual: %zu", totalLength, result.size());
        return "";
    }

    return result;
}

bool IsNumeric(const std::string& str)
{
    if (str.empty()) {
        return false;
    }
    for (char ch : str) {
        if (!std::isdigit(ch)) {
            return false;
        }
    }
    return true;
}

int StripLeadingZeros(const std::string& input)
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
        WRITE_LOG(LOG_FATAL, "StripLeadingZeros: invalid numeric string.");
        return -1;
    }
    
    long value = 0;
    auto [ptr, ec] = std::from_chars(numberStr.data(), numberStr.data() + numberStr.size(), value);
    if (ec != std::errc()) {
        value = 0;
    }
    return static_cast<int>(value);
}

std::vector<uint8_t> String2Uint8(const std::string& str, size_t len)
{
    std::vector<uint8_t> byteData(len);
    for (size_t i = 0; i < len; i++) {
        byteData[i] = static_cast<uint8_t>(str[i]);
    }
    return byteData;
}

std::string IntToStringWithPadding(int length, int maxLen)
{
    std::string str = std::to_string(length);
    if (str.length() > static_cast<size_t>(maxLen)) {
        return "";
    }
    return std::string(static_cast<size_t>(maxLen) - str.length(), '0') + str;
}

void SplitString(const std::string &origString, const std::string &seq,
                 std::vector<std::string> &resultStrings)
{
    if (seq.empty()) {
        return;
    }

    std::string::size_type p1 = 0;
    std::string::size_type p2 = origString.find(seq);

    while (p2 != std::string::npos) {
        if (p2 == p1) {
            ++p1;
            p2 = origString.find(seq, p1);
            continue;
        }
        resultStrings.push_back(origString.substr(p1, p2 - p1));
        p1 = p2 + seq.size();
        p2 = origString.find(seq, p1);
    }

    if (p1 != origString.size()) {
        resultStrings.push_back(origString.substr(p1));
    }
}

std::string SplicMessageStr(const std::string &str, const size_t methodType, const size_t methodVersion)
{
    if (str.empty()) {
        WRITE_LOG(LOG_FATAL, "Input string is empty.");
        return "";
    }
    const size_t bodyLen = str.size();
    size_t totalLength = MESSAGE_METHOD_POS + MESSAGE_METHOD_LEN +
                         MESSAGE_LENGTH_LEN + bodyLen;

    std::string messageMethodTypeStr = IntToStringWithPadding(methodType, MESSAGE_METHOD_LEN);
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
    result.push_back('0' + methodVersion);
    result.append(messageMethodTypeStr);
    result.append(messageBodyLen);
    result.append(str);
    if (result.size() != totalLength) {
        WRITE_LOG(LOG_FATAL, "size mismatch. Expected: %zu, Actual: %zu", totalLength, result.size());
        return "";
    }
    return result;
}

bool SendMessageByUnixSocket(const int sockfd, const std::string &messageStr)
{
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    size_t maxPathLen = sizeof(addr.sun_path) - 1;
    size_t pathLen = strlen(HDC_CREDENTIAL_SOCKET_SANDBOX_PATH.c_str());
    if (pathLen > maxPathLen) {
        WRITE_LOG(LOG_FATAL, "Socket path too long.");
        return false;
    }

    if (memcpy_s(addr.sun_path, maxPathLen, HDC_CREDENTIAL_SOCKET_SANDBOX_PATH.c_str(), pathLen) != 0) {
        WRITE_LOG(LOG_FATAL, "Failed to memcpy_st.");
        return false;
    }

    if (connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to connect to socket.");
        return false;
    }

    if (send(sockfd, messageStr.c_str(), messageStr.size(), 0) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to send message.");
        return false;
    }

    return true;
}

ssize_t RecvMessageByUnixSocket(const int sockfd, char data[], ssize_t size)
{
    ssize_t count = 0;
    ssize_t bytesRead = 0;
    while ((bytesRead = recv(sockfd, data + count, size - 1 - count, 0)) > 0) {
        count += bytesRead;
        if (count >= size - 1) {
            WRITE_LOG(LOG_FATAL, "Failed to read from socket.");
            return false;
        }
    }

    data[count] = '\0';
    if (bytesRead < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to read from socket.");
        return -1;
    }
    return count;
}

ssize_t GetCredential(const std::string &messageStr, char data[], ssize_t size)
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

    if (!SendMessageByUnixSocket(sockfd, messageStr)) {
        close(sockfd);
        return -1;
    }

    ssize_t count = RecvMessageByUnixSocket(sockfd, data, size);
    if (count < 0) {
        close(sockfd);
        return -1;
    }
    
    close(sockfd);
    return count;
}