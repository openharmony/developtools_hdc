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

#ifndef HDC_SUBSERVER_PROCESS_INFO_H
#define HDC_SUBSERVER_PROCESS_INFO_H

#include <memory>
#include <string>
#include <chrono>
#include "process_handle.h"

namespace Hdc {

class SubserverProcessInfo {
public:
    explicit SubserverProcessInfo(std::unique_ptr<ProcessHandle> handle);
    ~SubserverProcessInfo();

    SubserverProcessInfo(const SubserverProcessInfo&) = delete;
    SubserverProcessInfo& operator=(const SubserverProcessInfo&) = delete;

    SubserverStatus GetSubserverStatus();

private:
    std::unique_ptr<ProcessHandle> processHandle;
    std::chrono::steady_clock::time_point connectStartTime = std::chrono::steady_clock::now();
    mutable SubserverStatus status_ = SubserverStatus::UNKNOWN;
};

} // namespace Hdc

#endif // HDC_SUBSERVER_PROCESS_INFO_H