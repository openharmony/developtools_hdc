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
/*
############
This file is used to support compatibility between platforms, differences between old and new projects and
compilation platforms

defined HARMONY_PROJECT
With openharmony toolchains support. If not defined, it should be [device]buildroot or [PC]msys64(...)/ubuntu-apt(...)
envirments
############
*/
#include "system_depend.h"
#include "base.h"
#ifdef __OHOS__
#include "init_reboot.h"
#include "parameter.h"
#endif

namespace Hdc {
namespace SystemDepend {
bool GetDevItem(const char *key, string &out, const char *preDefine)
{
    bool ret = true;
    char tmpStringBuf[BUF_SIZE_MEDIUM] = "";
#ifdef HARMONY_PROJECT
    auto res = GetParameter(key, preDefine, tmpStringBuf, BUF_SIZE_MEDIUM);
    if (res <= 0) {
        return false;
    }
#else
    string sFailString = Base::StringFormat("Get parameter \"%s\" fail", key);
    string stringBuf = "param get " + string(key);
    Base::RunPipeComand(stringBuf.c_str(), tmpStringBuf, BUF_SIZE_MEDIUM - 1, true);
    if (!strcmp(sFailString.c_str(), tmpStringBuf)) {
        // failed
        ret = false;
        (void)memset_s(tmpStringBuf, BUF_SIZE_MEDIUM, 0, BUF_SIZE_MEDIUM);
    }
#endif
    out = tmpStringBuf;
    return ret;
}
}
}  // namespace Hdc
