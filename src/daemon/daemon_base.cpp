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
#ifndef UPDATER_MODE
#include "daemon_base.h"
#include <vector>
#include "base.h"
#include "define.h"
#include "os_account_manager.h"

namespace {
static const int32_t DEFAULT_USER_ID = 100;
}

namespace Hdc {
namespace HdcDaemonBase {
static int32_t GetUserId()
{
    std::vector<int32_t> ids;
    OHOS::ErrCode err = OHOS::AccountSA::OsAccountManager::QueryActiveOsAccountIds(ids);
    if (err != 0) {
        WRITE_LOG(LOG_FATAL, "QueryActiveOsAccountIds failed, err: %d", err);
        return 0;
    }
    if (ids.empty()) {
        WRITE_LOG(LOG_FATAL, "QueryActiveOsAccountIds is empty");
        return 0;
    }
    return ids[0];
}

bool CheckBundlePath(const std::string &bundleName, std::string &mountPath)
{
    int32_t currentUserId = GetUserId();
    if (currentUserId == 0) {
        WRITE_LOG(LOG_FATAL, "GetUserId failed, use default userId");
        currentUserId = DEFAULT_USER_ID;
    }
    std::string bundlePath = DEBUG_BUNDLE_PATH + std::to_string(currentUserId) + "/debug_hap/";
    if (access(bundlePath.c_str(), F_OK) != 0 || !Base::CheckBundleName(bundleName)) {
        WRITE_LOG(LOG_FATAL, "debug path %s not found", bundlePath.c_str());
        return false;
    }
    string targetPath = "";
    targetPath += bundlePath;
    targetPath += bundleName;
    if (access(targetPath.c_str(), F_OK) != 0) {
        WRITE_LOG(LOG_FATAL, "bundle mount path %s not found", targetPath.c_str());
        return false;
    }
    mountPath = targetPath;
    return true;
}
}
}
#endif
