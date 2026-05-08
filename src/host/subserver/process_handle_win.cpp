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

#ifdef _WIN32

#include "process_handle.h"
#include "define_plus.h"
#include "../host_common.h"

#include <windows.h>
#include <tlhelp32.h>

namespace Hdc {

struct ProcessHandle::ProcessHandleImpl {
    PROCESS_INFORMATION* pi = nullptr;

    ~ProcessHandleImpl()
    {
        if (pi) {
            if (pi->hThread) CloseHandle(pi->hThread);
            if (pi->hProcess) CloseHandle(pi->hProcess);
            delete pi;
        }
    }

    bool IsAlive() const
    {
        if (!pi || !pi->hProcess) return false;
        DWORD exitCode = 0;
        return GetExitCodeProcess(pi->hProcess, &exitCode) && exitCode == STILL_ACTIVE;
    }

    int GetExitCode() const
    {
        if (!pi || !pi->hProcess) return -1;
        DWORD exitCode = 0;
        GetExitCodeProcess(pi->hProcess, &exitCode);
        return static_cast<int>(exitCode);
    }

    bool IsValid() const
    {
        return pi && pi->hProcess;
    }
};

std::string ProcessHandle::GetExecutablePathImpl()
{
    std::string runPath = "";
    char* path = (char*)malloc(sizeof(char) * BUF_SIZE_SMALL);
    size_t nPathSize = BUF_SIZE_SMALL;
    int ret = uv_exepath(path, &nPathSize);
    if (ret < 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        uv_err_name_r(ret, buf, bufSize);
        WRITE_LOG(LOG_WARN, "uvexepath ret:%d error:%s", ret, buf);
        free(path);
        return "";
    }

    char shortPath[MAX_PATH] = "";
    std::string strPath = Base::UnicodeToUtf8(path, true);
    ret = GetShortPathName(strPath.c_str(), shortPath, MAX_PATH);
    runPath = shortPath;
    if (ret == 0) {
        int err = GetLastError();
        char buffer[1024] = { 0 };
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, err, 0, buffer, sizeof(buffer), NULL);
        WRITE_LOG(LOG_WARN, "GetShortPath errmsg:%s", buffer);
        runPath = std::string(path).substr(std::string(path).find_last_of("/\\") + 1);
    }
    WRITE_LOG(LOG_DEBUG, "server shortpath:[%s] runPath:[%s]",
              Hdc::MaskString(std::string(shortPath)).c_str(), Hdc::MaskString(runPath).c_str());
    free(path);
    return runPath;
}

std::string ProcessHandle::GetParentProcessNameImpl()
{
    std::string parentName;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return parentName;
    }

    DWORD currentPid = GetCurrentProcessId();
    DWORD parentPid = 0;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == currentPid) {
                parentPid = pe32.th32ParentProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    if (parentPid == 0) {
        CloseHandle(hSnapshot);
        return parentName;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == parentPid) {
                parentName = pe32.szExeFile;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return parentName;
}

bool ProcessHandle::BuildSubserverArgsImpl(char* buf, size_t bufSize, const char* listenString,
                                           const char* serial, const char* port)
{
    std::string logFileName = Base::GenerateSubserverLogFileName();
    if (logFileName.empty()) {
        return sprintf_s(buf, bufSize, "dummy -l 5 -s %s -m -N -i %s -o %s",
                         listenString, serial, port) >= 0;
    }
    return sprintf_s(buf, bufSize, "dummy -l 5 -s %s -m -N -i %s -o %s -L %s",
                     listenString, serial, port, logFileName.c_str()) >= 0;
}

uint32_t ProcessHandle::GetCurrentPidImpl()
{
    return GetCurrentProcessId();
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

std::unique_ptr<ProcessHandle> ProcessHandle::Spawn(const std::string& path, const char* args)
{
    auto handle = std::make_unique<ProcessHandle>();
    handle->processImpl_ = std::make_unique<ProcessHandleImpl>();
    handle->processImpl_->pi = new PROCESS_INFORMATION();

    STARTUPINFO si = {};
    si.cb = sizeof(STARTUPINFO);
#ifndef HDC_DEBUG
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
#endif

    if (!CreateProcess(path.c_str(), const_cast<char*>(args), nullptr, nullptr, false,
                       CREATE_NEW_CONSOLE, nullptr, nullptr, &si, handle->processImpl_->pi)) {
        WRITE_LOG(LOG_WARN, "CreateProcess failed with cmd:%s, args:%s, Error Code %d",
                  Hdc::MaskString(path).c_str(), args, GetLastError());
        delete handle->processImpl_->pi;
        handle->processImpl_->pi = nullptr;
        return nullptr;
    }

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

} // namespace Hdc

#endif // _WIN32