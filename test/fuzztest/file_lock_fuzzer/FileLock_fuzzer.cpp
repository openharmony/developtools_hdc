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

#include <cstdint>
#include <cstddef>
#include <string>
#include <unistd.h>

#include "file_lock_util.h"

namespace Hdc {
void FuzzFileLockAppend(const uint8_t* data, size_t size)
{
    if (size < 1 || size > 1000) {
        return;
    }

    std::string testPath = "/tmp/fuzz_test_" + std::to_string(getpid());
    std::string content(reinterpret_cast<const char*>(data), size);

    FileLockUtil::AtomicAppend(testPath, content);
    unlink(testPath.c_str());
}

void FuzzFileLockRemoveLine(const uint8_t* data, size_t size)
{
    if (size < 1 || size > 100) {
        return;
    }

    std::string testPath = "/tmp/fuzz_test_" + std::to_string(getpid());
    std::string lineToRemove(reinterpret_cast<const char*>(data), size);

    FileLockUtil::AtomicAppend(testPath, "line1\n");
    FileLockUtil::AtomicRemoveLine(testPath, lineToRemove);
    unlink(testPath.c_str());
}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Hdc::FuzzFileLockAppend(data, size);
    Hdc::FuzzFileLockRemoveLine(data, size);
    return 0;
}