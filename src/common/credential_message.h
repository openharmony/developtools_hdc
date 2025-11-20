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
#ifndef HDC_CREDENTIAL_MESSAGE_H
#define HDC_CREDENTIAL_MESSAGE_H

#include <string>
#include "log.h"
#include "securec.h"

class CredentialMessage {
public:
    CredentialMessage() = default;
    explicit CredentialMessage(const std::string& messageStr);
    void Init(const std::string& messageStr);
    ~CredentialMessage();
    int GetMessageVersion() const { return messageVersion; }
    int GetMessageMethodType() const { return messageMethodType; }
    int GetMessageBodyLen() const { return messageBodyLen; }
    const std::string& GetMessageBody() const { return messageBody; }

    void SetMessageVersion(int version);
    void SetMessageMethodType(int type) { messageMethodType = type; }
    void SetMessageBodyLen(int len) { messageBodyLen = len; }
    void SetMessageBody(const std::string& body);
    std::string Construct() const;

private:
    int messageVersion = 0;
    int messageMethodType = 0;
    int messageBodyLen = 0;
    std::string messageBody;
};

bool IsNumeric(const std::string& str);
int StripLeadingZeros(const std::string& input);
std::string IntToStringWithPadding(int length, int maxLen);
std::vector<uint8_t> String2Uint8(const std::string& str, size_t len);
void SplitString(const std::string &origString, const std::string &seq,
                 std::vector<std::string> &resultStrings);

constexpr size_t MESSAGE_STR_MAX_LEN = 1024;
constexpr size_t MESSAGE_VERSION_POS = 0;
constexpr size_t MESSAGE_METHOD_POS = 1;
constexpr size_t MESSAGE_METHOD_LEN = 3;
constexpr size_t MESSAGE_LENGTH_POS = 4;
constexpr size_t MESSAGE_LENGTH_LEN = 4;
constexpr size_t MESSAGE_BODY_POS = 8;

const size_t MESSAGE_PARAM_COMMAND_EVENT_REPORT_SIZE = 6;
const size_t MESSAGE_PARAM_RETURN_MAX_SIZE = 32;
const std::string HDC_COMMAND_REPORT = "usual.event.HDC_COMMAND_REPORT";
const std::string EVENT_PARAM_REPORT_USERID = "userId";
const std::string EVENT_PARAM_REPORT_ROLE = "role";
const std::string EVENT_PARAM_REPORT_STATUS = "status";
const std::string EVENT_PARAM_REPORT_TIME = "happenTime";
const std::string EVENT_PARAM_REPORT_COMMAND = "command";
const std::string EVENT_PARAM_REPORT_CONTENT = "content";
const std::string EVENT_PARAM_RETURN_SUCCESS = "success";
const std::string EVENT_PARAM_RETURN_FAILED = "failed";

enum V1MethodID {
    METHOD_ENCRYPT = 1,
    METHOD_DECRYPT,
};

enum MethodReport {
    METHOD_COMMAND_EVENT_REPORT = 1
};

enum MethodCommandEventReportParamIndex {
    PARAM_REPORT_USERID = 0,
    PARAM_REPORT_ROLE,
    PARAM_REPORT_TIME,
    PARAM_REPORT_STATUS,
    PARAM_REPORT_COMMAND,
    PARAM_REPORT_CONTENT,
};

enum MethodVersion {
    METHOD_VERSION_V1 = 1,
    METHOD_REPORT = 2,
    METHOD_VERSION_MAX = 9,
};

#endif // HDC_CREDENTIAL_MESSAGE_H