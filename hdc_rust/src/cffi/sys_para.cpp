/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

#include "parameter.h"
#include <string>

extern "C" int SetParameterEx(const char *key, const char *val)
{
    return SetParameter(key, val);
}

extern "C" int GetParameterEx(const char *key, const char *def, char *val, unsigned int len)
{
    return GetParameter(key, def, val, len);
}

extern "C" int WaitParameterEx(const char *key, const char *val, int timeout)
{
    return WaitParameter(key, val, timeout);
}

namespace Hdc {
bool SetDevItem(const char *key, const char *value)
{
    return SetParameterEx(key, value) == 0;
}

bool GetDevItem(const char *key, std::string &out, const char *preDefine)
{
    bool ret = true;
    constexpr uint16_t param_len = 512;
    char tmpStringBuf[param_len] = "";

    auto res = GetParameter(key, preDefine, tmpStringBuf, param_len);
    if (res <= 0) {
        return false;
    }
    out = tmpStringBuf;
    return ret;
}
}  // namespace Hdc