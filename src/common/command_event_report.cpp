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

#include "command_event_report.h"

#include <parameters.h>
#include "sys/socket.h"
#include "common.h"
#ifdef DAEMON_ONLY
#include <want.h>
#include <common_event_manager.h>
#include <common_event_publish_info.h>
#endif

using namespace OHOS;

namespace Hdc {
CommandEventReport::CommandEventReport()
{
}

CommandEventReport::~CommandEventReport()
{
}

bool CommandEventReport::IsSupportReport()
{
    return OHOS::system::GetParameter("const.pc_security.fileguard_force_enable", "") == "true";
}

std::string CommandEventReport::FormatMessage(const std::string &command, const std::string &raw,
    Base::Caller caller, bool isIntercepted)
{
    std::string content = raw;
    std::stringstream ss;
    std::string status = isIntercepted ? "forbidden" : "enabled";
    ss << std::to_string(getuid() / BASE_ID) << ";";
    ss << GetCallerName(caller) << ";";
    ss << GetCurrentTimeStamp() << ";";
    ss << status << ";";
    ss << command << ";";

    std::string result = ss.str();
    if (result.length() + content.length() >= MESSAGE_STR_MAX_LEN - MESSAGE_BODY_POS) {
        int maxContentSize = MESSAGE_STR_MAX_LEN - MESSAGE_BODY_POS - result.length();
        result.append(content.substr(content.length() - maxContentSize));
    } else {
        result.append(content);
    }

    return result;
}

std::string CommandEventReport::SplicMessageStr(const std::string &command, const std::string &raw,
    Base::Caller caller, bool isIntercepted)
{
    std::string str = FormatMessage(command, raw, caller, isIntercepted);
    if (str.empty()) {
        WRITE_LOG(LOG_FATAL, "Input command is empty.");
        return "";
    }

    std::string messageMethodTypeStr = IntToStringWithPadding(METHOD_COMMAND_EVENT_REPORT, MESSAGE_METHOD_LEN);
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
    const size_t bodyLen = str.size();
    size_t totalLength = MESSAGE_METHOD_POS + MESSAGE_METHOD_LEN +
                         MESSAGE_LENGTH_LEN + bodyLen;
    result.reserve(totalLength);
    result.push_back('0' + METHOD_REPORT);
    result.append(messageMethodTypeStr);
    result.append(messageBodyLen);
    result.append(str);
    if (result.size() != totalLength) {
        WRITE_LOG(LOG_FATAL, "size mismatch. Expected: %zu, Actual: %zu", totalLength, result.size());
        return "";
    }

    return result;
}

bool CommandEventReport::ReportCommandEvent(const std::string &inputRaw, Base::Caller caller,
    bool isIntercepted, std::string command)
{
#ifdef HDC_SUPPORT_REPORT_COMMAND_EVENT
    if (!IsSupportReport()) {
        return true;
    }

    if (command == "" && !GetCommandFromInputRaw(inputRaw.c_str(), command)) {
        return true;
    }

    if (caller == Base::Caller::DAEMON) {
        return Report(command, inputRaw, caller, isIntercepted);
    }

    return ReportByUnixSocket(command, inputRaw, caller, isIntercepted);
#else
    return true;
#endif
}

bool CommandEventReport::ReportFileCommandEvent(
    const std::string &localPath, bool master, bool serverOrDaemon)
{
#ifdef HDC_SUPPORT_REPORT_COMMAND_EVENT
    if (!IsSupportReport()) {
        return true;
    }

    std::string command = CMDSTR_FILE_SEND;
    if ((serverOrDaemon && !master) || (!serverOrDaemon && master)) {
        command = CMDSTR_FILE_RECV;
    }

    if (!serverOrDaemon) {
        return Report(command, localPath, Base::Caller::DAEMON, false);
    }

    return ReportByUnixSocket(command, localPath, Base::Caller::CLIENT, false);
#else
    return true;
#endif
}

bool CommandEventReport::Report(const std::string &command, const std::string &content,
    Base::Caller caller, bool isIntercepted)
{
#ifdef DAEMON_ONLY
    OHOS::EventFwk::CommonEventPublishInfo publishInfo;
    publishInfo.SetOrdered(true);
    OHOS::AAFwk::Want want;

    std::string status = isIntercepted ? "forbidden" : "enabled";
    want.SetAction(HDC_COMMAND_REPORT);
    want.SetParam(EVENT_PARAM_REPORT_USERID, int(getuid() / BASE_ID));
    want.SetParam(EVENT_PARAM_REPORT_ROLE, GetCallerName(caller));
    want.SetParam(EVENT_PARAM_REPORT_TIME, std::stoll(GetCurrentTimeStamp()));
    want.SetParam(EVENT_PARAM_REPORT_STATUS, status);
    want.SetParam(EVENT_PARAM_REPORT_COMMAND, command);
    want.SetParam(EVENT_PARAM_REPORT_CONTENT, content);

    OHOS::EventFwk::CommonEventData event {want};
    int32_t ret = OHOS::EventFwk::CommonEventManager::NewPublishCommonEvent(event, publishInfo);
    if (ret != 0) {
        WRITE_LOG(LOG_FATAL, "NewPublishCommonEvent error: %d.", ret);
        return false;
    }

    WRITE_LOG(LOG_DEBUG, "Report hdc command success.");
#endif
    return true;
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

bool RecvMessageByUnixSocket(const int sockfd)
{
    ssize_t count = 0;
    ssize_t bytesRead = 0;
    char buffer[MESSAGE_PARAM_RETURN_MAX_SIZE];
    while ((bytesRead = recv(sockfd, buffer + count, MESSAGE_PARAM_RETURN_MAX_SIZE - 1 - count, 0)) > 0) {
        count += bytesRead;
        if (count >= MESSAGE_PARAM_RETURN_MAX_SIZE - 1) {
            WRITE_LOG(LOG_FATAL, "Failed to read from socket.");
            return false;
        }
    }

    buffer[count] = '\0';
    std::string reciveStr(buffer, count);
    CredentialMessage messageStruct(reciveStr);
    if (messageStruct.GetMessageBody() == EVENT_PARAM_RETURN_FAILED) {
        WRITE_LOG(LOG_DEBUG, "Report hdc command failed.");
        return false;
    }

    return true;
}

bool CommandEventReport::ReportByUnixSocket(const std::string &command,
    const std::string &inputRaw, Base::Caller caller, bool isIntercepted)
{
    std::string messageStr = SplicMessageStr(command, inputRaw, caller, isIntercepted);
    if (messageStr.empty()) {
        WRITE_LOG(LOG_FATAL, "Failed to format message.");
        return false;
    }

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to create socket.");
        return false;
    }

    if (!SendMessageByUnixSocket(sockfd, messageStr)) {
        close(sockfd);
        return false;
    }

    if (!RecvMessageByUnixSocket(sockfd)) {
        close(sockfd);
        return false;
    }

    WRITE_LOG(LOG_DEBUG, "Report hdc command success.");
    return true;
}


bool CommandEventReport::GetCommandFromInputRaw(const char* inputRaw, std::string &command)
{
    std::vector<std::string> Compare = {
        CMDSTR_SOFTWARE_VERSION, CMDSTR_TARGET_MOUNT, CMDSTR_LIST_JDWP, CMDSTR_SHELL
    };
    std::vector<std::string> nCompare = {
        CMDSTR_SOFTWARE_HELP, CMDSTR_LIST_TARGETS, CMDSTR_SERVICE_KILL,
        CMDSTR_CHECK_SERVER, CMDSTR_CHECK_DEVICE, CMDSTR_HILOG,
        CMDSTR_WAIT_FOR, CMDSTR_CONNECT_TARGET, CMDSTR_SHELL_EX,
        CMDSTR_BUGREPORT, CMDSTR_SHELL + " ", CMDSTR_FILE_SEND,
        CMDSTR_FILE_RECV, CMDSTR_FORWARD_FPORT + " ", CMDSTR_FORWARD_RPORT + " ",
        CMDSTR_APP_INSTALL, CMDSTR_APP_UNINSTALL, CMDSTR_TRACK_JDWP,
        CMDSTR_TARGET_REBOOT, CMDSTR_TARGET_MODE
    };

    for (const auto &elem : Compare) {
        if (!strcmp(inputRaw, elem.c_str())) {
            command = elem;
            return true;
        }
    }
    for (const auto &elem : nCompare) {
        if (!strncmp(inputRaw, elem.c_str(), elem.size())) {
            command = elem;
            return true;
        }
    }

    return false;
}

std::string CommandEventReport::GetCurrentTimeStamp()
{
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    return std::to_string(seconds.count());
}

std::string CommandEventReport::GetCallerName(Base::Caller caller)
{
#ifdef HDC_HOST
    if (caller == Base::Caller::SERVER) {
        return Base::CALLER_SERVER;
    } else {
        return Base::CALLER_CLIENT;
    }
#else
    return Base::CALLER_DAEMON;
#endif
}
} // namespace Hdc