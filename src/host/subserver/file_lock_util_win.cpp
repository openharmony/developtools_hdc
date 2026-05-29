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

#include "file_lock_util.h"
#include "define_plus.h"
#include "log.h"
#include <windows.h>
#include <sstream>

namespace Hdc {

struct FileLockGuard::FileLockGuardImpl {
    HANDLE hFile = INVALID_HANDLE_VALUE;

    bool IsValid() const { return hFile != INVALID_HANDLE_VALUE; }
};

FileLockGuard::FileLockGuard() : lockImpl_(std::make_unique<FileLockGuardImpl>()) {}

FileLockGuard::~FileLockGuard()
{
    if (lockImpl_->IsValid()) {
        OVERLAPPED overlapped = {0};
        UnlockFileEx(lockImpl_->hFile, 0, 0, 0xFFFFFFFF, &overlapped);
        CloseHandle(lockImpl_->hFile);
    }
}

bool FileLockGuard::WithLock(const std::string& path, std::function<bool(FileLockGuard&)> callback)
{
    FileLockGuard guard;
    guard.lockImpl_->hFile = CreateFile(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                                        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (guard.lockImpl_->hFile == INVALID_HANDLE_VALUE) {
        WRITE_LOG(LOG_WARN, "CreateFile failed, path: %s, error: %lu",
                  path.c_str(), static_cast<unsigned long>(GetLastError()));
        return false;
    }

    OVERLAPPED overlapped = {0};
    if (!LockFileEx(guard.lockImpl_->hFile, LOCKFILE_EXCLUSIVE_LOCK, 0, 0, 0xFFFFFFFF, &overlapped)) {
        WRITE_LOG(LOG_WARN, "LockFileEx failed, error: %lu",
                  static_cast<unsigned long>(GetLastError()));
        CloseHandle(guard.lockImpl_->hFile);
        guard.lockImpl_->hFile = INVALID_HANDLE_VALUE;
        return false;
    }

    return callback(guard);
}

bool FileLockGuard::ReadAll(std::string& content)
{
    if (!lockImpl_->IsValid()) {
        return false;
    }

    constexpr size_t maxFileSize = 10 * 1024 * 1024; // 10MB
    DWORD fileSize = GetFileSize(lockImpl_->hFile, NULL);
    if (fileSize == 0) {
        return true;
    }
    if (fileSize > maxFileSize) {
        WRITE_LOG(LOG_WARN, "File too large: %lu bytes", static_cast<unsigned long>(fileSize));
        return false;
    }

    content.resize(fileSize);
    DWORD bytesRead = 0;
    OVERLAPPED overlapped = {0};

    BOOL success = ReadFile(lockImpl_->hFile, &content[0], fileSize, &bytesRead, &overlapped);
    if (!success || bytesRead != fileSize) {
        WRITE_LOG(LOG_WARN, "ReadAll failed or partial, expected: %lu, actual: %lu",
                  static_cast<unsigned long>(fileSize), static_cast<unsigned long>(bytesRead));
        return false;
    }
    return true;
}

bool FileLockGuard::Rewrite(const std::string& content)
{
    if (!lockImpl_->IsValid()) {
        return false;
    }

    OVERLAPPED overlapped = {0};
    overlapped.Offset = 0;
    overlapped.OffsetHigh = 0;
    SetEndOfFile(lockImpl_->hFile);

    if (content.empty()) {
        return true;
    }

    DWORD written = 0;
    BOOL success = WriteFile(lockImpl_->hFile, content.c_str(), content.size(), &written, NULL);
    if (!success || written != content.size()) {
        WRITE_LOG(LOG_WARN, "Rewrite failed or partial, expected: %llu, actual: %lu",
                  static_cast<unsigned long long>(content.size()), static_cast<unsigned long>(written));
        return false;
    }
    return true;
}

bool FileLockGuard::AppendLine(const std::string& content)
{
    if (!lockImpl_->IsValid()) {
        return false;
    }

    LARGE_INTEGER offset;
    offset.QuadPart = 0;
    SetFilePointerEx(lockImpl_->hFile, offset, NULL, FILE_END);

    DWORD written = 0;
    BOOL success = WriteFile(lockImpl_->hFile, content.c_str(), content.size(), &written, NULL);
    if (!success || written != content.size()) {
        WRITE_LOG(LOG_WARN, "AppendLine failed or partial, expected: %llu, actual: %lu",
                  static_cast<unsigned long long>(content.size()), static_cast<unsigned long>(written));
        return false;
    }
    return true;
}

bool FileLockGuard::Truncate()
{
    if (!lockImpl_->IsValid()) {
        return false;
    }

    LARGE_INTEGER offset;
    offset.QuadPart = 0;
    if (!SetFilePointerEx(lockImpl_->hFile, offset, NULL, FILE_BEGIN)) {
        return false;
    }

    BOOL result = SetEndOfFile(lockImpl_->hFile);

    return result;
}

bool FileLockGuard::FilterLine(const std::string& lineToRemove)
{
    std::string content;
    if (!ReadAll(content)) {
        return false;
    }

    std::string newContent;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty() || line == lineToRemove) {
            continue;
        }
        newContent += line + "\n";
    }

    return Rewrite(newContent);
}

} // namespace Hdc

#endif // _WIN32