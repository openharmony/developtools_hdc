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
#include "decompress.h"

#include <sstream>
#include <fstream>
#include <optional>
#include <iostream>

namespace Hdc {
bool Decompress::DecompressToLocal(std::string decPath)
{
    if (!CheckPath(decPath)) {
        return false;
    }
    uint8_t buff[HEADER_LEN];
    std::ifstream inFile(tarPath);
    std::optional<Entry> entry = std::nullopt;
    while (true) {
        inFile.read(reinterpret_cast<char*>(buff), HEADER_LEN);
        auto readcnt = inFile.gcount();
        if (readcnt == 0) {
            break;
        }
        if (inFile.fail() || readcnt != HEADER_LEN) {
            WRITE_LOG(LOG_FATAL, "read file error");
            break;
        }
        entry = Entry(buff, HEADER_LEN);
        if (!entry.value().CopyPayload(decPath, inFile)) {
            WRITE_LOG(LOG_FATAL, "CopyPayload fail");
            return false;
        }
        entry = std::nullopt;
        continue;
    }
    return true;
}

namespace {
void LogFatal(const char* format, const std::string& str)
{
    if (Base::GetCaller() == Base::Caller::CLIENT) {
        WRITE_LOG(LOG_FATAL, format, str.c_str());
    } else {
        WRITE_LOG(LOG_FATAL, format, Hdc::MaskString(str).c_str());
    }
}

bool ValidateTarFile(const std::string& tarPath, const uv_stat_t& stat)
{
    if (!(stat.st_mode & S_IFREG)) {
        LogFatal("%s not exist, or not file", tarPath);
        return false;
    }
    if (stat.st_size == 0 || stat.st_size % HEADER_LEN != 0) {
        LogFatal("file is not tar %s", tarPath);
        return false;
    }
    return true;
}

bool ValidateDecompressPath(const std::string& decPath, const uv_stat_t& stat)
{
    if (stat.st_mode & S_IFLNK) {
        LogFatal("path is a symlink, path traversal attack detected: %s", decPath);
        return false;
    }
    if (stat.st_mode & S_IFREG) {
        LogFatal("path exist, but is not a directory %s", decPath);
        return false;
    }
    return true;
}
}

bool Decompress::CheckPath(std::string decPath)
{
    uv_fs_t req;
    int rc = uv_fs_lstat(nullptr, &req, tarPath.c_str(), nullptr);
    uv_fs_req_cleanup(&req);
    if (rc != 0 || !ValidateTarFile(tarPath, req.statbuf)) {
        return false;
    }
    rc = uv_fs_lstat(nullptr, &req, decPath.c_str(), nullptr);
    uv_fs_req_cleanup(&req);
    if (rc == 0) {
        if (!ValidateDecompressPath(decPath, req.statbuf)) {
            return false;
        }
    } else {
        std::string estr;
        bool b = Base::TryCreateDirectory(decPath, estr);
        if (!b) {
            LogFatal("mkdir failed decPath:%s estr:%s", decPath);
            return false;
        }
    }
    return true;
}
}