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
#include "cmd.h"
#include "log.h"
#include "base.h"
#include "usb_util.h"
#include "selinux/selinux.h"
#include "parameter.h"
#include <string>
#include <cstdio>
#include <cerrno>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include "sys_para.h"

namespace Hdc {
using namespace std;

void SetSelinuxLabel()
{
#if defined(SURPPORT_SELINUX)
    char *con = nullptr;
    if (getcon(&con) != 0) {
        return;
    }
    if ((strcmp(con, "u:r:hdcd:s0") != 0) && (strcmp(con, "u:r:updater:s0") != 0)) {
        freecon(con);
        return;
    }
#ifdef HDC_BUILD_VARIANT_USER
    setcon("u:r:sh:s0");
#else
    string debugMode = "";
    string rootMode = "";
    string flashdMode = "";
    GetDevItem("const.debuggable", debugMode);
    GetDevItem("persist.hdc.root", rootMode);
    GetDevItem("updater.flashd.configfs", flashdMode);
    if ((debugMode == "1 " && rootMode == "1 ") || (debugMode == "1 " && flashdMode == "1 ")) {
        setcon("u:r:su:s0");
    } else {
        setcon("u:r:sh:s0");
    }
#endif
    freecon(con);
#endif
}

bool DropRootPrivileges()
{
    int ret;
    const char *userName = "shell";
    vector<const char *> groupsNames = { "shell", "log", "readproc" };
    struct passwd *user;
    gid_t *gids = nullptr;

    user = getpwnam(userName);
    if (user == nullptr) {
        WRITE_LOG(LOG_INFO, "getpwuid %s fail, %s", userName, strerror(errno));
        return false;
    }
    gids = static_cast<gid_t *>(calloc(groupsNames.size(), sizeof(gid_t)));
    if (gids == nullptr) {
        return false;
    }
    for (size_t i = 0; i < groupsNames.size(); i++) {
        struct group *group = getgrnam(groupsNames[i]);
        if (group == nullptr) {
            continue;
        }
        gids[i] = group->gr_gid;
    }
    ret = setuid(user->pw_uid);
    if (ret) {
        WRITE_LOG(LOG_WARN, "setuid %s fail, %s", userName, strerror(errno));
        free(gids);
        return false;
    }
    ret = setgid(user->pw_gid);
    if (ret) {
        WRITE_LOG(LOG_WARN, "setgid %s fail, %s", userName, strerror(errno));
        free(gids);
        return false;
    }
    ret = setgroups(groupsNames.size(), gids);
    if (ret) {
        WRITE_LOG(LOG_WARN, "setgroups %s fail, %s", userName, strerror(errno));
        free(gids);
        return false;
    }
    free(gids);
#if defined(SURPPORT_SELINUX)
    if (setcon("u:r:hdcd:s0") != 0) {
        WRITE_LOG(LOG_WARN, "setcon fail, errno %s", strerror(errno));
    }
#endif
    SetSelinuxLabel();
    return true;
}

extern "C"  bool NeedDropRootPrivileges()
{
    string rootMode;
    string debugMode;
    GetDevItem("const.debuggable", debugMode);
    GetDevItem("persist.hdc.root", rootMode);
    WRITE_LOG(LOG_WARN, "debuggable:[%s]", debugMode.c_str());
    WRITE_LOG(LOG_WARN, "param root:[%s]", rootMode.c_str());
    if (debugMode == "1") {
        if (rootMode == "1") {
            int rc = setuid(0);
            if (rc != 0) {
                char buffer[BUF_SIZE_DEFAULT] = { 0 };
                strerror_r(errno, buffer, BUF_SIZE_DEFAULT);
                WRITE_LOG(LOG_INFO, "setuid(0) fail %s", buffer);
                SetSelinuxLabel();
                _exit(0);
            }
            WRITE_LOG(LOG_INFO, "Root run rc:%d", rc);
        } else if (rootMode == "0") {
            if (getuid() == 0) {
                return DropRootPrivileges();
            }
        }
        // default keep root
    } else {
        return DropRootPrivileges();
    }
    return true;
}

extern "C" void Restart()
{
    execl("/system/bin/hdcd", "hdcd", nullptr, nullptr);
}
}
