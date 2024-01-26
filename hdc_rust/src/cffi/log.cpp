/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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
#include "log.h"
#ifdef HDC_HILOG
#include "hilog/log.h"
#endif

namespace Hdc {
void PrintLogEx(const char *functionName, int line, uint8_t logLevel, const char *msg, ...)
{
    char buf[BUF_SIZE_DEFAULT4] = { 0 }; // only 4k to avoid stack overflow in 32bit or L0
    va_list vaArgs;
    va_start(vaArgs, msg);
    const int retSize = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, msg, vaArgs);
    va_end(vaArgs);
    if (retSize < 0) {
        return;
    }

#ifdef  HDC_HILOG
    string tmpPath = functionName;
    string filePath = GetFileNameAny(tmpPath);
    static constexpr OHOS::HiviewDFX::HiLogLabel LOG_LABEL = {LOG_CORE, 0xD002D13, "HDC_LOG"};
    switch (static_cast<int>(logLevel)) {
        case static_cast<int>(LOG_DEBUG):
            // Info level log can be printed default in hilog, debug can't
            OHOS::HiviewDFX::HiLog::Info(LOG_LABEL, "[%{public}s:%{public}d] %{public}s",
                                         filePath.c_str(), line, buf);
            break;
        case static_cast<int>(LOG_INFO):
            OHOS::HiviewDFX::HiLog::Info(LOG_LABEL, "[%{public}s:%{public}d] %{public}s",
                                         filePath.c_str(), line, buf);
            break;
        case static_cast<int>(LOG_WARN):
            OHOS::HiviewDFX::HiLog::Warn(LOG_LABEL, "[%{public}s:%{public}d] %{public}s",
                                         filePath.c_str(), line, buf);
            break;
        case static_cast<int>(LOG_FATAL):
            OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "[%{public}s:%{public}d] %{public}s",
                                          filePath.c_str(), line, buf);
            break;
        default:
            break;
    }
#endif
}
}