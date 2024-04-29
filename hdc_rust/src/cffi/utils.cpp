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
#include <cstdio>
#include <cstring>
#include <random>
#include <sstream>
#include <thread>
#ifdef HDC_HILOG
#include "hilog/log.h"
#endif
#include <fcntl.h>
#ifdef _WIN32
#include <synchapi.h>
#include <errhandlingapi.h>
#include <winerror.h>
#include <handleapi.h>
#else
#include <pthread.h>
#endif
#include <unistd.h>
#include <securec.h>

constexpr uint16_t BUF_SIZE_TINY = 64;
constexpr uint16_t BUF_SIZE_DEFAULT = 1024;
constexpr int ERR_API_FAIL = -13000;
constexpr int ERR_BUF_OVERFLOW = -9998;
constexpr int ERR_FILE_OPEN = -11000;
constexpr int ERR_FILE_WRITE = -10996;
constexpr int ERR_FILE_STAT = -10997;

extern "C" {
    static char GetPathSep()
    {
#ifdef _WIN32
        const char sep = '\\';
#else
        const char sep = '/';
#endif
        return sep;
    }

#ifdef _WIN32
    // return value: <0 error; 0 can start new server instance; >0 server already exists
    __declspec(dllexport) int ProgramMutex(const char* procname, bool checkOrNew, const char* tmpDir)
    {
        char bufPath[BUF_SIZE_DEFAULT] = "";
        if (tmpDir == nullptr) {
            return ERR_API_FAIL;
        }

        if (snprintf_s(bufPath, sizeof(bufPath), sizeof(bufPath) - 1, "%s%c.%s.pid",
                       tmpDir, GetPathSep(), procname) < 0) {
            return ERR_BUF_OVERFLOW;
        }

        char pidBuf[BUF_SIZE_TINY] = "";
        int pid = static_cast<int>(getpid());
        if (snprintf_s(pidBuf, sizeof(pidBuf), sizeof(pidBuf) - 1, "%d", pid) < 0) {
            return ERR_BUF_OVERFLOW;
        }

        FILE *fp = fopen(bufPath, "a+");
        if (fp == nullptr) {
            return ERR_FILE_OPEN;
        }

        char buf[BUF_SIZE_DEFAULT] = "";
        if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "Global\\%s", procname) < 0) {
            fclose(fp);
            return ERR_BUF_OVERFLOW;
        }
        HANDLE hMutex = CreateMutex(nullptr, FALSE, buf);
        DWORD dwError = GetLastError();
        if (ERROR_ALREADY_EXISTS == dwError || ERROR_ACCESS_DENIED == dwError) {
            fclose(fp);
            return 1;
        }
        if (checkOrNew) {
            CloseHandle(hMutex);
        }

        int fd = fileno(fp);
        int rc = ftruncate(fd, 0);
        if (rc == -1) {
            fclose(fp);
            return ERR_FILE_STAT;
        }
        rc = fwrite(&pidBuf, sizeof(char), strlen(pidBuf), fp);
        if (rc == -1) {
            fclose(fp);
            return ERR_FILE_WRITE;
        }

        if (checkOrNew) {
            fclose(fp);
        }

        return 0;
    }

#else
    // return value: <0 error; 0 can start new server instance; >0 server already exists
    int ProgramMutex(const char* procname, bool checkOrNew, const char* tmpDir)
    {
        char bufPath[BUF_SIZE_DEFAULT] = "";
        if (tmpDir == nullptr) {
            return ERR_API_FAIL;
        }

        if (snprintf_s(bufPath, sizeof(bufPath), sizeof(bufPath) - 1, "%s%c.%s.pid",
                       tmpDir, GetPathSep(), procname) < 0) {
            return ERR_BUF_OVERFLOW;
        }

        char pidBuf[BUF_SIZE_TINY] = "";
        int pid = static_cast<int>(getpid());
        if (snprintf_s(pidBuf, sizeof(pidBuf), sizeof(pidBuf) - 1, "%d", pid) < 0) {
            return ERR_BUF_OVERFLOW;
        }

        int fd = open(bufPath, O_RDWR | O_CREAT, 0666);
        if (fd < 0) {
            return ERR_FILE_OPEN;
        }

        struct flock fl;
        fl.l_type = F_WRLCK;
        fl.l_start = 0;
        fl.l_whence = SEEK_SET;
        fl.l_len = 0;
        int retChild = fcntl(fd, F_SETLK, &fl);
        if (retChild == -1) {
            close(fd);
            return 1;
        }

        int rc = ftruncate(fd, 0);
        if (rc == -1) {
            close(fd);
            return ERR_FILE_STAT;
        }
        rc = write(fd, &pidBuf, strlen(pidBuf));
        if (rc == -1) {
            close(fd);
            return ERR_FILE_WRITE;
        }

        if (checkOrNew) {
            close(fd);
        }

        return 0;
    }
#endif
}

