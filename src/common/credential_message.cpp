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

using namespace Hdc;

CredentialMessage::CredentialMessage(const std::string& messageStr)
{
    if (messageStr.empty() || messageStr.length() < MESSAGE_BODY_POS) {
        WRITE_LOG(LOG_FATAL, "messageStr is too short!");
        return;
    }

    int versionInt = messageStr[MESSAGE_VERSION_POS] - '0';
    if (!IsInRange(versionInt, METHOD_VERSION_V1, METHOD_VERSION_MAX)) {
        WRITE_LOG(LOG_FATAL, "Invalid message version %d.", versionInt);
        return;
    }

    messageVersion = versionInt;

    std::string messageMethodStr = messageStr.substr(MESSAGE_METHOD_POS, MESSAGE_METHOD_LEN);
    messageMethodType = StripLeadingZeros(messageMethodStr);

    std::string messageLengthStr = messageStr.substr(MESSAGE_LENGTH_POS, MESSAGE_LENGTH_LEN);
    char* end = nullptr;
    size_t bodyLength = strtol(messageLengthStr.c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || bodyLength > MESSAGE_STR_MAX_LEN) {
        WRITE_LOG(LOG_FATAL, "Invalid message body length %s.", messageLengthStr.c_str());
        return;
    }

    if (messageStr.length() < MESSAGE_BODY_POS + bodyLength) {
        WRITE_LOG(LOG_FATAL, "messageStr is too short: %s", messageStr.c_str());
        return;
    }

    messageBodyLen = static_cast<int>(bodyLength);
    messageBody = messageStr.substr(MESSAGE_BODY_POS, bodyLength);
}
CredentialMessage::~CredentialMessage()
{
    if (!messageBody.empty()) {
        memset_s(&messageBody[0], messageBody.size(), 0, messageBody.size());
        messageBody.clear();
    }
}

std::string CredentialMessage::Construct() const
{
    size_t totalLength = 0;
    totalLength += 1;
    totalLength += MESSAGE_METHOD_LEN;
    totalLength += MESSAGE_LENGTH_LEN;
    totalLength += messageBody.size();

    std::string messageMethodTypeStr = ConvertInt(messageMethodType, MESSAGE_METHOD_LEN);
    if (messageMethodTypeStr.size() != MESSAGE_METHOD_LEN) {
        WRITE_LOG(LOG_FATAL, "messageMethod length Error!");
        return "";
    }

    std::string messageBodyLenStr = ConvertInt(messageBodyLen, MESSAGE_LENGTH_LEN);
    if (messageBodyLenStr.size() > MESSAGE_LENGTH_LEN) {
        WRITE_LOG(LOG_FATAL, "messageBodyLen length must be:%d,now is:%s",
            MESSAGE_LENGTH_LEN, messageBodyLenStr.c_str());
        return "";
    }
    
    std::vector<char> newBuffer(totalLength + 1, '\0');
    if (snprintf_s(newBuffer.data(), newBuffer.size(), newBuffer.size() - 1, "%c%s%s%s", ('0' + messageVersion),
        messageMethodTypeStr.c_str(), messageBodyLenStr.c_str(), messageBody.c_str())
        < 0) {
        WRITE_LOG(LOG_FATAL, "Construct Error!");
        return "";
    }

    std::string result(newBuffer.data(), totalLength);
    return result;
}

bool IsNumeric(const std::string& str)
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
    
    char* end = nullptr;
    long value = strtol(numberStr.c_str(), &end, 10);
    return static_cast<int>(value);
}

inline bool IsInRange(int value, int min, int max)
{
    return (value >= min && value <= max);
}

std::vector<uint8_t> String2Uint8(const std::string& str, size_t len)
{
    std::vector<uint8_t> byteData(len);
    for (size_t i = 0; i < len; i++) {
        byteData[i] = static_cast<uint8_t>(str[i]);
    }
    return byteData;
}

std::string ConvertInt(int len, int maxLen)
{
    std::string str = std::to_string(len);
    if (str.length() > static_cast<size_t>(maxLen)) {
        return "";
    }
    return std::string(maxLen - str.length(), '0') + str;
}