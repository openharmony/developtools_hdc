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

#ifndef HDC_COMMAND_EVENT_REPORT_H
#define HDC_COMMAND_EVENT_REPORT_H

#include <memory>
#include <singleton.h>

#include "base.h"
#include "credential_message.h"

namespace Hdc {
using namespace OHOS;

const int32_t BASE_ID = 200000;
const std::string HDC_CREDENTIAL_SOCKET_SANDBOX_PATH = "/data/hdc/hdc_huks/hdc_credential.socket";

class CommandEventReport : public std::enable_shared_from_this<CommandEventReport> {
    DECLARE_DELAYED_SINGLETON(CommandEventReport)
public:
    bool ReportCommandEvent(const std::string &inputRaw, Base::Caller caller,
        bool isIntercepted, std::string command = "");
    bool ReportFileCommandEvent(
        const std::string &localPath, bool master, bool serverOrDaemon);
private:
    bool IsSupportReport();
    std::string SplicMessageStr(const std::string &command, const std::string &raw,
        Base::Caller caller, bool isIntercepted);
    std::string FormatMessage(const std::string &command, const std::string &raw,
        Base::Caller caller, bool isIntercepted);
    bool GetCommandFromInputRaw(const char* inputRaw, std::string &command);
    bool Report(const std::string &command, const std::string &content,
        Base::Caller caller, bool isIntercepted);
    bool ReportByUnixSocket(const std::string &command, const std::string &inputRaw,
        Base::Caller caller, bool isIntercepted);
    std::string GetCurrentTimeStamp();
    std::string GetCallerName(Base::Caller caller);
};
} // namespace Hdc
#endif // HDC_COMMAND_EVENT_REPORT_H