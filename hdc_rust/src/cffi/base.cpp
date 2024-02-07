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
#include "base.h"

namespace Hdc {
const std::string StringFormat(const char * const formater, va_list &vaArgs)
{
    std::vector<char> args(GetMaxBufSize());
    const int retSize = vsnprintf_s(args.data(), GetMaxBufSize(), GetMaxBufSize() - 1, formater, vaArgs);
    if (retSize < 0) {
        return std::string("");
    } else {
        return std::string(args.data(), retSize);
    }
}

const std::string StringFormat(const char * const formater, ...)
{
    va_list vaArgs;
    va_start(vaArgs, formater);
    std::string ret = StringFormat(formater, vaArgs);
    va_end(vaArgs);
    return ret;
}

bool RunPipeComand(const char *cmdString, char *outBuf, uint16_t sizeOutBuf, bool ignoreTailLf)
{
    FILE *pipeHandle = popen(cmdString, "r");
    if (pipeHandle == nullptr) {
        return false;
    }
    int bytesRead = 0;
    int bytesOnce = 0;
    while (!feof(pipeHandle)) {
        bytesOnce = fread(outBuf, 1, sizeOutBuf - bytesRead, pipeHandle);
        if (bytesOnce <= 0) {
            break;
        }
        bytesRead += bytesOnce;
    }
    if (bytesRead && ignoreTailLf) {
        if (outBuf[bytesRead - 1] == '\n') {
            outBuf[bytesRead - 1] = '\0';
        }
    }
    pclose(pipeHandle);
    return bytesRead;
}
}