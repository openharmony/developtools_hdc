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
#include "daemon_base_wrapper.h"
#include "sys_para.h"
#include <securec.h>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {
constexpr const char *DEBUG_BASE = "/mnt/debug/";
constexpr const char *DEBUG_HAP_SUBDIR = "/debug_hap/";
constexpr const char *PARAM_ACTIVE_USER = "persist.sys.active_user";
constexpr int DEFAULT_USER_ID = 100;

int QueryActiveOsAccount()
{
    std::string val;
    Hdc::GetDevItem(PARAM_ACTIVE_USER, val, "100");
    int userId = std::atoi(val.c_str());
    return userId > 0 ? userId : DEFAULT_USER_ID;
}
}

extern "C" {
int DaemonBaseGetActiveUserId()
{
    return QueryActiveOsAccount();
}

int DaemonBaseCheckBundlePath(const char *bundleName, char *outPath, int maxLen)
{
    if (bundleName == nullptr || outPath == nullptr) {
        return -1;
    }
    int userId = QueryActiveOsAccount();
    std::string path = std::string(DEBUG_BASE) + std::to_string(userId) +
                       DEBUG_HAP_SUBDIR + bundleName;
    if (static_cast<int>(path.size()) >= maxLen) {
        return -1;
    }
    if (strncpy_s(outPath, maxLen, path.c_str(), path.size()) != EOK) { return -1; }
    outPath[maxLen - 1] = '\0';
#ifndef _WIN32
    if (access(outPath, F_OK) != 0) {
        return -1;
    }
#endif
    return static_cast<int>(path.size());
}
}
