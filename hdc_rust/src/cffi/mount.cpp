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
#include "log.h"
#include "mount.h"

#include <sys/mount.h>
#include <securec.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace Hdc;

bool FindMountDeviceByPath(const char *toQuery, char *dev)
{
    int fd;
    int res;
    int ret;
    char *token = nullptr;
    const char delims[] = "\n";
    char buf[BUF_SIZE_DEFAULT2];

    fd = open("/proc/mounts", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    ret = read(fd, buf, sizeof(buf) - 1);
    WRITE_LOG(LOG_FATAL, "FindMountDeviceByPath read %d bytes\n", ret);
    while (ret > 0) {
        buf[sizeof(buf) - 1] = '\0';
        token = strtok(buf, delims);

        while (token) {
            char dir[BUF_SIZE_SMALL] = "";
            int freq;
            int passnno;
            // clang-format off
            res = sscanf_s(token, "%255s %255s %*s %*s %d %d\n", dev, BUF_SIZE_SMALL - 1,
                           dir, BUF_SIZE_SMALL - 1, &freq, &passnno);
            // clang-format on
            dev[BUF_SIZE_SMALL - 1] = '\0';
            dir[BUF_SIZE_SMALL - 1] = '\0';
            if (res == 4 && (strcmp(toQuery, dir) == 0)) {  // 4 : The correct number of parameters
                close(fd);
                return true;
            }
            token = strtok(nullptr, delims);
        }
        ret = read(fd, buf, sizeof(buf) - 1);
        WRITE_LOG(LOG_FATAL, "FindMountDeviceByPath read again %d bytes", ret);
    }
    if (ret < 0) {
        WRITE_LOG(LOG_FATAL, "read failed, return %d", ret);
    }

    close(fd);
    return false;
}

bool RemountPartition(const char *dir)
{
    int fd;
    int off = 0;
    char dev[BUF_SIZE_SMALL] = "";

    if (!FindMountDeviceByPath(dir, dev) || strlen(dev) < 4) {  // 4 : file count
        return false;
    }

    if ((fd = open(dev, O_RDONLY | O_CLOEXEC)) < 0) {
        return false;
    }
    ioctl(fd, BLKROSET, &off);
    close(fd);

    if (mount(dev, dir, "none", MS_REMOUNT, nullptr) < 0) {
        return false;
    }
    return true;
}

bool RemountDevice()
{
    if (getuid() != 0) {
        return false;
    }
    struct stat info;
    if (!lstat("/vendor", &info) && (info.st_mode & S_IFMT) == S_IFDIR) {
        // has vendor
        if (!RemountPartition("/vendor")) {
            return false;
        }
    }
    if (!lstat("/data", &info) && (info.st_mode & S_IFMT) == S_IFDIR) {
        if (!RemountPartition("/data")) {
            return false;
        }
    }
    return true;
}