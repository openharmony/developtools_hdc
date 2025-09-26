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
            entry = std::nullopt;
            break;
        }
        entry = std::nullopt;
        continue;
    }
    inFile.close();
    return true;
}

bool Decompress::CheckPath(std::string decPath)
{
    uv_fs_t req;
    int rc = uv_fs_lstat(nullptr, &req, tarPath.c_str(), nullptr);
    uv_fs_req_cleanup(&req);
    if (rc != 0 || !(req.statbuf.st_mode & S_IFREG)) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_FATAL, "%s not exist, or not file", tarPath.c_str());
        } else {
            WRITE_LOG(LOG_FATAL, "%s not exist, or not file", Hdc::MaskString(tarPath).c_str());
        }
        return false;
    }
    auto fileSize = req.statbuf.st_size;
    if (fileSize == 0 || fileSize % HEADER_LEN != 0) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_FATAL, "file is not tar %s", tarPath.c_str());
        } else {
            WRITE_LOG(LOG_FATAL, "file is not tar %s", Hdc::MaskString(tarPath).c_str());
        }
        return false;
    }
    rc = uv_fs_lstat(nullptr, &req, decPath.c_str(), nullptr);
    uv_fs_req_cleanup(&req);
    if (rc == 0) {
        if (req.statbuf.st_mode & S_IFREG) {
            if (Base::GetCaller() == Base::Caller::CLIENT) {
                WRITE_LOG(LOG_FATAL, "path is exist, and path not dir %s", decPath.c_str());
            } else {
                WRITE_LOG(LOG_FATAL, "path is exist, and path not dir %s", Hdc::MaskString(decPath).c_str());
            }
            return false;
        }
    } else {
        std::string estr;
        bool b = Base::TryCreateDirectory(decPath, estr);
        if (!b) {
            if (Base::GetCaller() == Base::Caller::CLIENT) {
                WRITE_LOG(LOG_FATAL, "mkdir failed decPath:%s estr:%s", decPath.c_str(), estr.c_str());
            } else {
                WRITE_LOG(LOG_FATAL, "mkdir failed decPath:%s estr:%s", Hdc::MaskString(decPath).c_str(), estr.c_str());
            }
            return false;
        }
    }
    return true;
}
}