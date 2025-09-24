/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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
#include <charconv>
#include "credential_base.h"

using namespace Hdc;

namespace HdcCredentialBase {
char GetPathSep()
{
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    return sep;
}

int RemoveDir(const std::string& dir)
{
    DIR *pdir = opendir(dir.c_str());
    if (pdir == nullptr) {
        WRITE_LOG(LOG_FATAL, "opendir failed, error is:%s", strerror(errno));
        return -1;
    }
    struct dirent *ent;
    struct stat st;
    while ((ent = readdir(pdir)) != nullptr) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        std::string subpath = dir + GetPathSep() + ent->d_name;
        if (lstat(subpath.c_str(), &st) == -1) {
            WRITE_LOG(LOG_WARN, "lstat failed subpath:%s", ent->d_name);
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (RemoveDir(subpath) == -1) {
                closedir(pdir);
                return -1;
            }
            rmdir(subpath.c_str());
        } else if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
            if (unlink(subpath.c_str()) == -1) {
                WRITE_LOG(LOG_FATAL, "Failed to unlink file or symlink, error is :%s", strerror(errno));
            }
        } else {
            WRITE_LOG(LOG_DEBUG, "lstat st_mode:%07o", st.st_mode);
        }
    }
    if (rmdir(dir.c_str()) == -1) {
        closedir(pdir);
        return -1;
    }
    closedir(pdir);
    return 0;
}

int RemovePath(const std::string& path)
{
    struct stat st;
    if (lstat(path.c_str(), &st) == -1) {
        WRITE_LOG(LOG_WARN, "lstat failed path:%s", strerror(errno));
        return -1;
    }
    if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
        if (unlink(path.c_str()) == -1) {
            WRITE_LOG(LOG_FATAL, "Failed to unlink file or symlink,, error is :%s", strerror(errno));
        }
    } else if (S_ISDIR(st.st_mode)) {
        if (path == "." || path == "..") {
            return 0;
        }
        int rc = RemoveDir(path);
        return rc;
    }
    WRITE_LOG(LOG_DEBUG, "Directory removed successfully.");
    return 0;
}

const std::string StringFormat(const char* const formater, ...)
{
    va_list vaArgs;
    va_start(vaArgs, formater);
    std::string ret = StringFormat(formater, vaArgs);
    va_end(vaArgs);
    return ret;
}

const std::string StringFormat(const char* const formater, va_list& vaArgs)
{
    std::vector<char> args(MAX_SIZE_IOBUF_STABLE);
    const int retSize = vsnprintf_s(
        args.data(), MAX_SIZE_IOBUF_STABLE, (args.size() >= 1) ? (args.size() - 1) : 0, formater, vaArgs);
    if (retSize < 0) {
        return std::string("");
    } else {
        return std::string(args.data(), retSize);
    }
}

bool CreatePathWithMode(const char* path, mode_t mode)
{
    if (::mkdir(path, mode) != 0) {
        WRITE_LOG(LOG_FATAL, "Failed to create directory ,error is :%s", strerror(errno));
        return false;
    }
    if (::chmod(path, mode) != 0) {
        WRITE_LOG(LOG_FATAL, "Failed to set directory permissions, error is :%s", strerror(errno));
        return false;
    }
    WRITE_LOG(LOG_DEBUG, "Directory created successfully.");
    return true;
}

bool IsUserDir(const std::string& dir)
{
    int userId = 0;
    auto [ptr, ec] = std::from_chars(dir.data(), dir.data() + dir.size(), userId);
    if (ec != std::errc()) {
        userId = 0;
    }
    return userId >= MIN_USER_ID && userId <= MAX_USER_ID;
}
}   // namespace HdcCredentialBase