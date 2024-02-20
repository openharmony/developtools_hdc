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
// ############################# enum define ###################################
#ifndef HDC_LOG_H
#define HDC_LOG_H

#include <cinttypes>
#include <cstdarg>
#include <string>
#include "base.h"

namespace Hdc {
using namespace std;

enum LogLevel {
    LOG_OFF,
    LOG_FATAL,
    LOG_WARN,
    LOG_INFO,  // default
    LOG_DEBUG,
    LOG_ALL,
    LOG_VERBOSE,
    LOG_LAST = LOG_VERBOSE,  // tail, not use
};

inline string GetFileNameAny(const string &path)
{
    string tmpString = path;
    size_t tmpNum = tmpString.rfind('/');
    if (tmpNum == std::string::npos) {
        tmpNum = tmpString.rfind('\\');
        if (tmpNum == std::string::npos) {
            return tmpString;
        }
    }
    tmpString = tmpString.substr(tmpNum + 1, tmpString.size() - tmpNum);
    return tmpString;
}

void PrintLogEx(const char *functionName, int line, uint8_t logLevel, const char *msg, ...);

#define WRITE_LOG(level, fmt, ...)   PrintLogEx(__FILE__, __LINE__, level, fmt, ##__VA_ARGS__)
}
#endif
