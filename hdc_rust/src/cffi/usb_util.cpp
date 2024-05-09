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
#include <cstdarg>
#include "securec.h"
#include "usb_util.h"
#include "log.h"

#ifdef HOST_MINGW
#include <windows.h>
#endif
using namespace Hdc;
constexpr auto USB_FFS_BASE = "/dev/usb-ffs/";

std::string GetDevPath(const std::string &path)
{
    DIR *dir = ::opendir(path.c_str());
    if (dir == nullptr) {
        WRITE_LOG(LOG_WARN, "%s: cannot open devpath: errno: %d\n", path.c_str(), errno);
        return "";
    }

    std::string res = USB_FFS_BASE;
    std::string node;
    int count = 0;
    struct dirent *entry = nullptr;
    while ((entry = ::readdir(dir))) {
        if (*entry->d_name == '.') {
            continue;
        }
        node = entry->d_name;
        ++count;
    }
    if (count > 1) {
        res += "hdc";
    } else {
        res += node;
    }
    ::closedir(dir);
    return res;
}

