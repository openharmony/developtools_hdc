/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "DecompressToLocal_fuzzer.h"

namespace Hdc {

static int RemoveDir(const string &dir)
{
    DIR *pdir = opendir(dir.c_str());
    if (pdir == nullptr) {
        WRITE_LOG(LOG_WARN, "opendir failed dir:%s", dir.c_str());
        return -1;
    }
    struct dirent *ent;
    struct stat st;
    while ((ent = readdir(pdir)) != nullptr) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        std::string subpath = dir + Base::GetPathSep() + ent->d_name;
        if (lstat(subpath.c_str(), &st) == -1) {
            WRITE_LOG(LOG_WARN, "lstat failed subpath:%s", subpath.c_str());
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (RemoveDir(subpath) == -1) {
                closedir(pdir);
                return -1;
            }
            rmdir(subpath.c_str());
        } else if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
            unlink(subpath.c_str());
        } else {
            WRITE_LOG(LOG_WARN, "lstat st_mode:%07o subpath:%s", st.st_mode, subpath.c_str());
        }
    }
    if (rmdir(dir.c_str()) == -1) {
        closedir(pdir);
        return -1;
    }
    closedir(pdir);
    return 0;
}

static void RemovePath(const string &path)
{
    struct stat st;
    if (lstat(path.c_str(), &st) == -1) {
        WRITE_LOG(LOG_WARN, "lstat failed path:%s", path.c_str());
        return;
    }
    if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
        unlink(path.c_str());
    } else if (S_ISDIR(st.st_mode)) {
        if (path == "." || path == "..") {
            return;
        }
        int rc = RemoveDir(path);
        if (rc < 0) {
            WRITE_LOG(LOG_WARN, "RemoveDir rc:%d path:%s", rc, path.c_str());
        }
    }
}

static string DecompressFromTar(const char *path)
{
    string dir;
    string tarpath = path;
    string::size_type rindex = tarpath.rfind(".tar");
    if (rindex != string::npos) {
        dir = tarpath.substr(0, rindex) + Base::GetPathSep();
        Decompress dc(tarpath);
        dc.DecompressToLocal(dir);
    }
    return dir;
}

static string GenerateTarFile(const uint8_t *data, size_t size)
{
    string name = Base::GetRandomString(EXPECTED_LEN);
    string path = "./" + name + ".tar";
    FILE *fp = fopen(path.c_str(), "wb");
    if (!fp) {
        WRITE_LOG(LOG_WARN, "fopen failed %s", path.c_str());
        return path;
    }

    Header header;
    header.UpdataSize(size);
    header.UpdataFileType(TypeFlage::ORDINARYFILE);
    header.UpdataName(name + ".data");
    char buff[HEADER_LEN] = { 0 };
    header.GetBytes(reinterpret_cast<uint8_t *>(buff), HEADER_LEN);
    size_t count = fwrite(buff, sizeof(uint8_t), HEADER_LEN, fp);
    if (count < 0) {
        WRITE_LOG(LOG_WARN, "fwrite count:%zd error:%d", count, ferror(fp));
    }
    count = fwrite(data, sizeof(uint8_t), size, fp);
    if (count < 0) {
        WRITE_LOG(LOG_WARN, "fwrite count:%zd error:%d", count, ferror(fp));
    }
    auto pading = HEADER_LEN - size % HEADER_LEN;
    if (pading < HEADER_LEN) {
        char pad[HEADER_LEN] = { 0 };
        fwrite(pad, sizeof(uint8_t), pading, fp);
    }
    int rc = fclose(fp);
    if (rc != 0) {
        WRITE_LOG(LOG_WARN, "fclose rc:%d errno:%d", rc, errno);
    }
    return path;
}

bool FuzzDecompressToLocal(const uint8_t *data, size_t size)
{
    if (size <= 0) {
        return true;
    }
    string tarpath = GenerateTarFile(data, size);
    string tardir = DecompressFromTar(tarpath.c_str());
    RemovePath(tardir);
    RemovePath(tarpath);
    return true;
}
} // namespace Hdc

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    Hdc::FuzzDecompressToLocal(data, size);
    return 0;
}
