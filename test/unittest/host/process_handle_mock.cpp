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
#include "define_plus.h"
#include "../host_common.h"
#include "log.h"
#include "base.h"
#include "define.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <fcntl.h>
#include <csignal>
#include <sstream>
#include <fstream>
#include <vector>
#include <functional>

static std::function<bool(pid_t)> mockIsAliveFunc;
static std::function<int(pid_t)> mockGetExitCodeFunc;
void SetMockIsAlive(std::function<bool(pid_t)> func)
{
    mockIsAliveFunc = func;
}

void SetMockGetExitCode(std::function<int(pid_t)> func)
{
    mockGetExitCodeFunc = func;
}

void ClearMockFunctions()
{
    mockIsAliveFunc = nullptr;
    mockGetExitCodeFunc = nullptr;
}

namespace Hdc {

struct ProcessHandle::ProcessHandleImpl {
    pid_t pid = -1;

    bool IsAlive() const
    {
        if (pid <= 0) return false;

        if (mockIsAliveFunc) {
            return mockIsAliveFunc(pid);
        }

        return kill(pid, 0) == 0;
    }

    int GetExitCode() const
    {
        if (pid <= 0) return -1;

        if (mockGetExitCodeFunc) {
            return mockGetExitCodeFunc(pid);
        }

        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -1;
    }

    bool IsValid() const
    {
        return pid > 0;
    }
};

ProcessHandle::ProcessHandle() : processImpl_(std::make_unique<ProcessHandleImpl>()) {}
ProcessHandle::~ProcessHandle() = default;

ProcessHandle::ProcessHandle(ProcessHandle&& other) noexcept
    : processImpl_(std::move(other.processImpl_)) {}

ProcessHandle& ProcessHandle::operator=(ProcessHandle&& other) noexcept
{
    if (this != &other) {
        processImpl_ = std::move(other.processImpl_);
    }
    return *this;
}

bool ProcessHandle::IsAlive() const
{
    return processImpl_ ? processImpl_->IsAlive() : false;
}

int ProcessHandle::GetExitCode() const
{
    return processImpl_ ? processImpl_->GetExitCode() : -1;
}

bool ProcessHandle::IsValid() const
{
    return processImpl_ && processImpl_->IsValid();
}

void ProcessHandle::SetPidForTest(pid_t pid)
{
    if (processImpl_) {
        processImpl_->pid = pid;
    }
}

pid_t ProcessHandle::GetPidForTest() const
{
    return processImpl_ ? processImpl_->pid : -1;
}

std::string ProcessHandle::GetExecutablePathImpl()
{
    return "/tmp/mock_hdc";
}

std::string ProcessHandle::GetParentProcessNameImpl()
{
    return "hdc";
}

uint32_t ProcessHandle::GetCurrentPidImpl()
{
    return getpid();
}

bool ProcessHandle::BuildSubserverArgsImpl(char* buf, size_t bufSize, const char* listenString,
                                           const char* serial, const char* port)
{
    return sprintf_s(buf, bufSize, "-l 5 -s %s -m -N -i %s -o %s",
                     listenString, serial, port) >= 0;
}

std::unique_ptr<ProcessHandle> ProcessHandle::SpawnSubprocess(const std::string& path, const char* args)
{
    auto handle = std::make_unique<ProcessHandle>();
    handle->processImpl_->pid = getpid() + 1;
    return handle;
}

} // namespace Hdc
