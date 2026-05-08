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

#include "file_lock_util.h"

namespace Hdc {

bool FileLockUtil::AtomicAppend(const std::string& path, const std::string& content)
{
    return FileLockGuard::WithLock(path, [&content](FileLockGuard& guard) {
        return guard.AppendLine(content);
    });
}

bool FileLockUtil::AtomicRemoveLine(const std::string& path, const std::string& lineToRemove)
{
    return FileLockGuard::WithLock(path, [&lineToRemove](FileLockGuard& guard) {
        return guard.FilterLine(lineToRemove);
    });
}

bool FileLockUtil::AtomicReadAndClear(const std::string& path, std::string& content)
{
    bool result = FileLockGuard::WithLock(path, [&content](FileLockGuard& guard) {
        if (!guard.ReadAll(content)) {
            return false;
        }
        bool truncateResult = guard.Truncate();
        return truncateResult;
    });
    return result;
}

} // namespace Hdc