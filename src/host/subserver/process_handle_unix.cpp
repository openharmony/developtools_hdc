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

#ifndef _WIN32

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
#include <memory>

#ifdef __APPLE__
#include <libproc.h>
#endif

namespace Hdc {

struct ProcessHandle::ProcessHandleImpl {
    pid_t pid = -1;

    bool IsAlive() const
    {
        if (pid <= 0) {
            return false;
        }
        return kill(pid, 0) == 0;
    }

    int GetExitCode() const
    {
        if (pid <= 0) {
            return -1;
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

std::string ProcessHandle::GetExecutablePathImpl()
{
    std::unique_ptr<char[]> path(new (std::nothrow) char[BUF_SIZE_SMALL]);
    if (path == nullptr) {
        WRITE_LOG(LOG_WARN, "Failed to allocate memory for path");
        return "";
    }
    size_t nPathSize = BUF_SIZE_SMALL;
    int ret = uv_exepath(path.get(), &nPathSize);
    if (ret < 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        uv_err_name_r(ret, buf, bufSize);
        WRITE_LOG(LOG_WARN, "uvexepath ret:%d error:%s", ret, buf);
        return "";
    }
    return path.get();
}

std::string ProcessHandle::GetParentProcessNameImpl()
{
    std::string parentName;
#ifdef __APPLE__
    pid_t parentPid = getppid();
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(parentPid, pathbuf, sizeof(pathbuf)) > 0) {
        std::string fullPath = pathbuf;
        size_t pos = fullPath.find_last_of('/');
        if (pos != std::string::npos) {
            parentName = fullPath.substr(pos + 1);
        } else {
            parentName = fullPath;
        }
    }
#else
    pid_t parentPid = getppid();
    std::string path = "/proc/" + std::to_string(parentPid) + "/comm";
    std::ifstream file(path);
    if (file.is_open()) {
        std::getline(file, parentName);
    }
#endif
    return parentName;
}

bool ProcessHandle::BuildSubserverArgsImpl(char* buf, size_t bufSize, const char* listenString,
                                           const char* serial, const char* port)
{
    std::string logFileName = Base::GenerateSubserverLogFileName();
    if (logFileName.empty()) {
        return sprintf_s(buf, bufSize, "-l 0 -s %s -m -N -i %s -o %s",
                         listenString, serial, port) >= 0;
    }
    return sprintf_s(buf, bufSize, "-l %d -s %s -m -N -i %s -o %s -L %s",
                     Base::GetLogLevelByEnv(), listenString, serial, port, logFileName.c_str()) >= 0;
}

uint32_t ProcessHandle::GetCurrentPidImpl()
{
    return getpid();
}

ProcessHandle::ProcessHandle() : processImpl_(nullptr) {}

ProcessHandle::~ProcessHandle() = default;

ProcessHandle::ProcessHandle(ProcessHandle&& other) noexcept : processImpl_(std::move(other.processImpl_)) {}

ProcessHandle& ProcessHandle::operator=(ProcessHandle&& other) noexcept
{
    if (this != &other) {
        processImpl_ = std::move(other.processImpl_);
    }
    return *this;
}

std::unique_ptr<ProcessHandle> ProcessHandle::SpawnSubprocess(const std::string& path, const char* args)
{
    auto handle = std::make_unique<ProcessHandle>();
    handle->processImpl_ = std::make_unique<ProcessHandleImpl>();

    std::vector<std::string> argList;
    std::vector<char*> argv;

    argList.push_back(path);

    if (args && strlen(args) > 0) {
        std::istringstream iss(args);
        std::string arg;
        while (iss >> arg) {
            argList.push_back(arg);
        }
    }

    for (auto& a : argList) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        WRITE_LOG(LOG_WARN, "fork failed, error: %d", errno);
        return nullptr;
    }

    if (pid == 0) {
        Base::CloseOpenFd();
        execv(path.c_str(), argv.data());
        _exit(1);
    }

    handle->processImpl_->pid = pid;
    return handle;
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

#ifdef HDC_UNIT_TEST
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
#endif

} // namespace Hdc

#endif // !_WIN32