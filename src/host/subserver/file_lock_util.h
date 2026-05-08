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

#ifndef HDC_FILE_LOCK_UTIL_H
#define HDC_FILE_LOCK_UTIL_H

#include <string>
#include <functional>
#include <memory>

namespace Hdc {

class FileLockGuard {
public:
    static bool WithLock(const std::string& path,
                         std::function<bool(FileLockGuard&)> callback);

    bool ReadAll(std::string& content);
    bool Rewrite(const std::string& content);
    bool AppendLine(const std::string& content);
    bool Truncate();
    bool FilterLine(const std::string& lineToRemove);

    FileLockGuard(const FileLockGuard&) = delete;
    FileLockGuard& operator=(const FileLockGuard&) = delete;

private:
    FileLockGuard();
    ~FileLockGuard();

    struct FileLockGuardImpl;
    std::unique_ptr<FileLockGuardImpl> lockImpl_;

    friend class FileLockUtil;
};

class FileLockUtil {
public:
    static bool AtomicAppend(const std::string& path, const std::string& content);
    static bool AtomicRemoveLine(const std::string& path, const std::string& lineToRemove);
    static bool AtomicReadAndClear(const std::string& path, std::string& content);
};

} // namespace Hdc

#endif // HDC_FILE_LOCK_UTIL_H