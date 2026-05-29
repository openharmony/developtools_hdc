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

#include "file_lock_util.h"
#include "define_plus.h"
#include "log.h"
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>
#include <sstream>

namespace Hdc {

struct FileLockGuard::FileLockGuardImpl {
    int fd = -1;

    bool IsValid() const { return fd >= 0; }
};

FileLockGuard::FileLockGuard() : lockImpl_(std::make_unique<FileLockGuardImpl>()) {}

FileLockGuard::~FileLockGuard()
{
    if (lockImpl_->IsValid()) {
        flock(lockImpl_->fd, LOCK_UN);
        close(lockImpl_->fd);
    }
}

bool FileLockGuard::WithLock(const std::string& path, std::function<bool(FileLockGuard&)> callback)
{
    FileLockGuard guard;
    guard.lockImpl_->fd = open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (guard.lockImpl_->fd < 0) {
        WRITE_LOG(LOG_WARN, "open failed, path: %s, error: %d", path.c_str(), errno);
        return false;
    }

    if (flock(guard.lockImpl_->fd, LOCK_EX) < 0) {
        WRITE_LOG(LOG_WARN, "flock LOCK_EX failed, error: %d", errno);
        close(guard.lockImpl_->fd);
        guard.lockImpl_->fd = -1;
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
    off_t fileSize = lseek(lockImpl_->fd, 0, SEEK_END);
    if (fileSize <= 0) {
        return true;
    }
    if (static_cast<size_t>(fileSize) > maxFileSize) {
        WRITE_LOG(LOG_WARN, "File too large: %lld bytes", static_cast<long long>(fileSize));
        return false;
    }

    lseek(lockImpl_->fd, 0, SEEK_SET);
    content.resize(fileSize);

    ssize_t bytesRead = read(lockImpl_->fd, &content[0], fileSize);
    if (bytesRead < 0 || static_cast<size_t>(bytesRead) != fileSize) {
        WRITE_LOG(LOG_WARN, "ReadAll failed or partial, expected: %lld, actual: %lld",
                  static_cast<long long>(fileSize), static_cast<long long>(bytesRead));
        return false;
    }
    return true;
}

bool FileLockGuard::Rewrite(const std::string& content)
{
    if (!lockImpl_->IsValid()) {
        return false;
    }

    ftruncate(lockImpl_->fd, 0);
    lseek(lockImpl_->fd, 0, SEEK_SET);

    if (content.empty()) {
        return true;
    }

    ssize_t written = write(lockImpl_->fd, content.c_str(), content.size());
    if (written < 0 || static_cast<size_t>(written) != content.size()) {
        WRITE_LOG(LOG_WARN, "Rewrite failed or partial, expected: %zu, actual: %zd",
                  content.size(), written);
        return false;
    }
    return true;
}

bool FileLockGuard::AppendLine(const std::string& content)
{
    if (!lockImpl_->IsValid()) {
        return false;
    }

    lseek(lockImpl_->fd, 0, SEEK_END);

    ssize_t written = write(lockImpl_->fd, content.c_str(), content.size());
    if (written < 0 || static_cast<size_t>(written) != content.size()) {
        WRITE_LOG(LOG_WARN, "AppendLine failed or partial, expected: %zu, actual: %zd",
                  content.size(), written);
        return false;
    }
    return true;
}

bool FileLockGuard::Truncate()
{
    if (!lockImpl_->IsValid()) {
        return false;
    }
    return ftruncate(lockImpl_->fd, 0) >= 0;
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

#endif // !_WIN32