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
#include <sys/wait.h>

using namespace Hdc;

bool FindMountDeviceByPath(const char *toQuery, char *dev)
{
    int ret = false;
    int len = BUF_SIZE_DEFAULT2;
    char buf[BUF_SIZE_DEFAULT2];

    FILE *fp = fopen("/proc/mounts", "r");
    if (fp == nullptr) {
        WRITE_LOG(LOG_FATAL, "fopen /proc/mounts error:%d", errno);
        return false;
    }

    while (fgets(buf, len, fp) != nullptr) {
        char dir[BUF_SIZE_SMALL] = "";
        int freq;
        int passnno;
        int res = 0;
        // clang-format off
        res = sscanf_s(buf, "%255s %255s %*s %*s %d %d\n", dev, BUF_SIZE_SMALL - 1,
                       dir, BUF_SIZE_SMALL - 1, &freq, &passnno);
        // clang-format on
        dev[BUF_SIZE_SMALL - 1] = '\0';
        dir[BUF_SIZE_SMALL - 1] = '\0';
        if (res == 4 && (strcmp(toQuery, dir) == 0)) {  // 4 : The correct number of parameters
            WRITE_LOG(LOG_DEBUG, "FindMountDeviceByPath dev:%s dir:%s", dev, dir);
            ret = true;
            break;
        }
    }
    int rc = fclose(fp);
    if (rc != 0) {
        WRITE_LOG(LOG_WARN, "fclose rc:%d error:%d", rc, errno);
    }
    if (!ret) {
        WRITE_LOG(LOG_FATAL, "FindMountDeviceByPath not found %s", toQuery);
    }
    return ret;
}

bool RemountPartition(const char *dir)
{
    int fd;
    int off = 0;
    int ret = 0;
    char dev[BUF_SIZE_SMALL] = "";

    if (!FindMountDeviceByPath(dir, dev) || strlen(dev) < 4) {  // 4 : file count
        WRITE_LOG(LOG_FATAL, "FindMountDeviceByPath dir:%s failed", dir);
        return false;
    }

    if ((fd = open(dev, O_RDONLY | O_CLOEXEC)) < 0) {
        WRITE_LOG(LOG_FATAL, "open dev:%s failed, error:%d", dev, errno);
        return false;
    }
    ioctl(fd, BLKROSET, &off);
    close(fd);

    ret = mount(dev, dir, "none", MS_REMOUNT, nullptr); 
    if (ret < 0) {
        WRITE_LOG(LOG_FATAL, "mount %s failed, reason is %s", dev, strerror(errno));
        return false;
    }
    return true;
}

bool RemountDevice()
{
    if (getuid() != 0) {
        return false;
    }
    bool ret = true;
    struct stat info;
    if (!lstat("/vendor", &info) && (info.st_mode & S_IFMT) == S_IFDIR) {
        if (!RemountPartition("/vendor")) {
            WRITE_LOG(LOG_FATAL, "Mount failed /vendor");
            ret = false;
        }
    }
    if (!lstat("/system", &info) && (info.st_mode & S_IFMT) == S_IFDIR) {
        if (!RemountPartition("/")) {
            WRITE_LOG(LOG_FATAL, "Mount failed /system");
            ret = false;
        }
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        WRITE_LOG(LOG_FATAL, "Fork failed");
        return false;
    } else if (pid == 0) {
        execl("/bin/remount", "remount", (char*)NULL);
        perror("execl remount failed");
        _exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            WRITE_LOG(LOG_FATAL, "Remount via binary failed with exit code: %d", WEXITSTATUS(status));
            ret = false;
        }
    }
    return ret;
}