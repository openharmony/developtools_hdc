/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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

#include "subserver_process_info.h"
#include "define_plus.h"

namespace Hdc {

SubserverProcessInfo::SubserverProcessInfo(std::unique_ptr<ProcessHandle> handle)
    : processHandle(std::move(handle)) {}

SubserverProcessInfo::~SubserverProcessInfo() = default;

SubserverStatus SubserverProcessInfo::GetSubserverStatus()
{
    SubserverStatus currentStatus = SubserverStatus::UNKNOWN;
    if (!processHandle || !processHandle->IsValid()) {
        currentStatus = SubserverStatus::SUBPROCESS_FAIL;
    } else if (processHandle->IsAlive()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - connectStartTime);
        currentStatus = elapsed.count() >= SUBSERVER_CONNECT_TIMEOUT
            ? SubserverStatus::CONNECT_SUCCESS
            : SubserverStatus::CONNECTING;
    } else {
        int exitCode = processHandle->GetExitCode();
        if (exitCode >= static_cast<int>(SubserverStatus::CONNECTING) &&
            exitCode < static_cast<int>(SubserverStatus::UNKNOWN)) {
            currentStatus = static_cast<SubserverStatus>(exitCode);
        } else {
            currentStatus = SubserverStatus::SUBSERVER_OTHER_EXIT;
        }
    }
    status_ = currentStatus;
    return currentStatus;
}

} // namespace Hdc