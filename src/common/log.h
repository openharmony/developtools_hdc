/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#ifndef __H_HDC_LOG_H__
#define __H_HDC_LOG_H__

#include <cinttypes>

namespace Hdc {

namespace Base {
    void PrintLogEx(const char *functionName, int line, uint8_t logLevel, const char *msg, ...);
}

enum HdcLogLevel {
    LOG_OFF,
    LOG_FATAL,
    LOG_WARN,
    LOG_INFO,  // default
    LOG_DEBUG,
    LOG_ALL,
    LOG_VERBOSE,
    LOG_LAST = LOG_VERBOSE,  // tail, not use
};

#ifdef IS_RELEASE_VERSION
#define WRITE_LOG(level, fmt, ...)   Base::PrintLogEx(__FUNCTION__, __LINE__, level, fmt, ##__VA_ARGS__)
#else
#define WRITE_LOG(level, fmt, ...)   Base::PrintLogEx(__FILE_NAME__, __LINE__, level, fmt, ##__VA_ARGS__)
#endif

#ifndef HDC_HOST
#define WRITE_LOG_DAEMON(level, fmt, ...) WRITE_LOG(level, fmt, ##__VA_ARGS__)
#else
#define WRITE_LOG_DAEMON(level, fmt, ...)
#endif

#ifdef HDC_DEBUG
#define DEBUG_LOG(fmt, ...)   WRITE_LOG(LOG_DEBUG, fmt, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...)
#endif

}  // namespace Hdc
#endif  // __H_HDC_LOG_H__
