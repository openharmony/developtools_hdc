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

#include "process_handle.h"

namespace Hdc {

std::string ProcessHandle::GetExecutablePath()
{
    return GetExecutablePathImpl();
}

std::string ProcessHandle::GetParentProcessName()
{
    return GetParentProcessNameImpl();
}

uint32_t ProcessHandle::GetCurrentPid()
{
    return GetCurrentPidImpl();
}

bool ProcessHandle::BuildSubserverArgs(char* buf, size_t bufSize, const char* listenString,
                                       const char* serial, const char* port)
{
    return BuildSubserverArgsImpl(buf, bufSize, listenString, serial, port);
}

} // namespace Hdc