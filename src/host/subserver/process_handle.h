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

#ifndef HDC_PROCESS_HANDLE_H
#define HDC_PROCESS_HANDLE_H

#include <memory>
#include <string>
#include <cstdint>

namespace Hdc {

enum class SubserverStatus : int32_t {
    CONNECTING = 1001,
    SUBPROCESS_FAIL = 1002,
    PORT_LISTEN_FAIL = 1003,
    CONNECT_TIMEOUT = 1004,
    INVALID_DEVICE = 1005,
    PARAM_ERROR = 1006,
    CONNECT_SUCCESS = 1007,
    USB_DISCONNECT = 1008,
    SUBSERVER_ABONDON = 1009,
    SUBSERVER_OTHER_EXIT = 1010,
    UNKNOWN = 1099,
};

constexpr int32_t SUBSERVER_CONNECT_TIMEOUT = 5;

class ProcessHandle {
public:
    ProcessHandle();
    ~ProcessHandle();
    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
    ProcessHandle(ProcessHandle&& other) noexcept;
    ProcessHandle& operator=(ProcessHandle&& other) noexcept;

    bool IsAlive() const;
    bool IsValid() const;
    int GetExitCode() const;

    static std::unique_ptr<ProcessHandle> SpawnSubprocess(const std::string& path, const char* args);

    static std::string GetExecutablePath();
    static std::string GetParentProcessName();
    static uint32_t GetCurrentPid();
    static bool BuildSubserverArgs(char* buf, size_t bufSize, const char* listenString,
                                   const char* serial, const char* port);

private:
    struct ProcessHandleImpl;
    std::unique_ptr<ProcessHandleImpl> processImpl_;

    static std::string GetExecutablePathImpl();
    static std::string GetParentProcessNameImpl();
    static uint32_t GetCurrentPidImpl();
    static bool BuildSubserverArgsImpl(char* buf, size_t bufSize, const char* listenString,
                                       const char* serial, const char* port);
};

} // namespace Hdc

#endif // HDC_PROCESS_HANDLE_H