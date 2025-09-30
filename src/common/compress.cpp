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
#include "compress.h"

#include <fstream>

namespace Hdc {
bool Compress::AddPath(std::string path)
{
    uv_fs_t req;
    int rc = uv_fs_lstat(nullptr, &req, path.c_str(), nullptr);
    uv_fs_req_cleanup(&req);
    if (rc != 0) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_FATAL, "%s is not exist", path.c_str());
        } else {
            WRITE_LOG(LOG_FATAL, "%s is not exist", Hdc::MaskString(path).c_str());
        }
        return false;
    }
    if (req.statbuf.st_mode & S_IFREG) {
        return AddEntry(path);
    }
    if (!AddEntry(path)) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_DEBUG, "AddEntry failed dir:%s", path.c_str());
        } else {
            WRITE_LOG(LOG_DEBUG, "AddEntry failed dir:%s", Hdc::MaskString(path).c_str());
        }
        return false;
    }
    rc = uv_fs_scandir(nullptr, &req, path.c_str(), 0, nullptr);
    if (rc < 0) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_DEBUG, "uv_fs_scandir failed dir:%s", path.c_str());
        } else {
            WRITE_LOG(LOG_DEBUG, "uv_fs_scandir failed dir:%s", Hdc::MaskString(path).c_str());
        }
        uv_fs_req_cleanup(&req);
        return false;
    }
    uv_dirent_t dent;
    while (uv_fs_scandir_next(&req, &dent) != UV_EOF) {
        if (strncmp(dent.name, ".", 1) == 0 || strncmp(dent.name, "..", 2) == 0) {
            continue;
        }
        string subpath = path + Base::GetPathSep() + dent.name;
        if (!AddPath(subpath)) {
            uv_fs_req_cleanup(&req);
            return false;
        }
    }
    uv_fs_req_cleanup(&req);
    return true;
}

bool Compress::AddEntry(std::string path)
{
    if (this->maxcount > 0 && this->entrys.size() > this->maxcount) {
        WRITE_LOG(LOG_FATAL, "Entry.size %zu exceeded maximum %zu", entrys.size(), maxcount);
        return false;
    }
    if (this->prefix.length() > 0 && path == this->prefix) {
        WRITE_LOG(LOG_DEBUG, "Ignoring compressed root directory");
        return true;
    }
    Entry entry(this->prefix, path);
    if (Base::GetCaller() == Base::Caller::CLIENT) {
        WRITE_LOG(LOG_DEBUG, "AddEntry %s", path.c_str());
    } else {
        WRITE_LOG(LOG_DEBUG, "AddEntry %s", Hdc::MaskString(path).c_str());
    }
    entrys.push_back(entry);
    return true;
}

bool Compress::SaveToFile(std::string localPath)
{
    if (localPath.length() <= 0) {
        localPath = "tmp.tar";
    }
    uv_fs_t req;
    int rc = uv_fs_lstat(nullptr, &req, localPath.c_str(), nullptr);
    if (rc == 0 && req.statbuf.st_mode & S_IFDIR) {
        return false;
    }

    std::ofstream file(localPath.c_str(), std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_FATAL, "SaveToFile open file %s failed", localPath.c_str());
        } else {
            WRITE_LOG(LOG_FATAL, "SaveToFile open file %s failed", Hdc::MaskString(localPath).c_str());
        }
        return false;
    }
    WRITE_LOG(LOG_DEBUG, "SaveToFile entrys len : %u", entrys.size());
    for (auto& entry : entrys) {
        entry.WriteToTar(file);
    }
    file.close();
    return true;
}

void Compress::UpdataPrefix(std::string pathPrefix)
{
    this->prefix = pathPrefix;
}

void Compress::UpdataMaxCount(size_t maxCount)
{
    this->maxcount = maxCount;
}
}
