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
#include "entry.h"

#include <iostream>
#include <fstream>
#include <optional>

namespace Hdc {

constexpr size_t ENTRY_FILE_BUFSIZE = 4 * 1024; // 4KB
constexpr uint64_t ENTRY_MAX_FILE_SIZE = static_cast<uint64_t>(4) * 1024 * 1024 * 1024; // 4GB
std::optional<std::string> StripPrefix(const std::string& str, const std::string& prefix)
{
    if (str.compare(0, prefix.length(), prefix) == 0) {
        auto p_path = str.substr(prefix.length());
        return p_path;
    } else {
        return std::nullopt;
    }
}

Entry::Entry(std::string prefix, std::string path)
{
    this->prefix = prefix + Base::GetPathSep();
    uv_fs_t req;
    int rc = uv_fs_lstat(nullptr, &req, path.c_str(), nullptr);
    uv_fs_req_cleanup(&req);
    if (rc == 0) {
        if (req.statbuf.st_mode & S_IFDIR) {
            header.UpdataFileType(TypeFlage::DIRECTORY);
            header.UpdataSize(0);
        } else if (req.statbuf.st_mode & S_IFREG) {
            auto fileSize = req.statbuf.st_size;
            if (fileSize < ENTRY_MAX_FILE_SIZE) { // max package size is 4GB
                header.UpdataSize(fileSize);
                needSize = fileSize;
                header.UpdataFileType(TypeFlage::ORDINARYFILE);
            } else {
#ifdef HDC_HOST
                Base::PrintMessage("[Warning]File: %s, size: %lldB, over the 4GB limit, ignored.",
                    path.c_str(), fileSize);
#else
                WRITE_LOG(LOG_WARN, "File: %s, size: %lldB, over the 4GB limit, ignored.",
                    Hdc::MaskString(path).c_str(), fileSize);
#endif
            }
        }
    }
    UpdataName(path);
}

Entry::Entry(uint8_t data[512], int dataLen)
{
    header = Header(data, dataLen);
    needSize = header.Size();
}


void Entry::AddData(uint8_t *data, size_t len)
{
    if (this->needSize == 0) {
        return;
    }
    if (this->needSize > len) {
        for (size_t i = 0; i < len; i++) {
            this->data.push_back(data[i]);
        }
        this->needSize -= len;
    } else {
        for (size_t i = 0; i < this->needSize; i++) {
            this->data.push_back(data[i]);
        }
        this->needSize = 0;
    }
}

std::string Entry::GetName()
{
    auto name = this->prefix + this->header.Name();
    return name;
}

bool Entry::UpdataName(std::string name)
{
    if (!this->prefix.empty()) {
        auto p_path = Hdc::StripPrefix(name, this->prefix);
        if (p_path.has_value()) {
            return this->header.UpdataName(p_path.value());
        }
    }
    return this->header.UpdataName(name);
}

bool Entry::CopyPayload(std::string prefixPath, std::ifstream &inFile)
{
    switch (this->header.FileType()) {
        case TypeFlage::ORDINARYFILE: {
            if (!PayloadToFile(prefixPath, inFile)) {
                return false;
            }
            break;
        }
        case TypeFlage::DIRECTORY: {
            if (!PayloadToDir(prefixPath)) {
                return false;
            }
            break;
        }
        default:
            return false;
    }
    return true;
}

bool Entry::PayloadToFile(std::string prefixPath, std::ifstream &inFile)
{
    std::string saveFile = "";
    saveFile = prefixPath + GetName();
    std::ofstream outFile(saveFile, std::ios::app | std::ios::binary);
    if (!outFile.is_open()) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_FATAL, "PayloadToFile open %s fail", saveFile.c_str());
        } else {
            WRITE_LOG(LOG_FATAL, "PayloadToFile open %s fail", Hdc::MaskString(saveFile).c_str());
        }
        return false;
    }
    bool ret = true;
    uint8_t *buffAppend = new uint8_t[ENTRY_FILE_BUFSIZE];
    while (this->needSize >= ENTRY_FILE_BUFSIZE) {
        ret = ReadAndWriteData(inFile, outFile, buffAppend, ENTRY_FILE_BUFSIZE, ENTRY_FILE_BUFSIZE);
        if (!ret) {
            break;
        }
        this->needSize -= ENTRY_FILE_BUFSIZE;
    }
    if (ret && this->needSize > 0) {
        long int paddingSize = HEADER_LEN - (this->needSize % HEADER_LEN);
        long int lastBufSize = (paddingSize == HEADER_LEN) ? this->needSize :
            this->needSize + paddingSize;
        ret = ReadAndWriteData(inFile, outFile, buffAppend, lastBufSize, this->needSize);
    }
    delete[] buffAppend;
    buffAppend = nullptr;
    outFile.close();
    this->needSize = 0;
    return ret;
}

bool Entry::ReadAndWriteData(std::ifstream &inFile, std::ofstream &outFile, uint8_t *buffAppend,
    int readSize, int writeSize)
{
    if (buffAppend == nullptr) {
        WRITE_LOG(LOG_FATAL, "ReadAndWriteData buffAppend is null");
        return false;
    }
    inFile.read(reinterpret_cast<char*>(buffAppend), readSize);
    auto readcnt = inFile.gcount();
    if (inFile.fail() || readcnt != readSize) {
        WRITE_LOG(LOG_FATAL, "ReadAndWriteData read file error");
        return false;
    }
    outFile.write(reinterpret_cast<const char*>(buffAppend), writeSize);
    if (outFile.fail()) {
        WRITE_LOG(LOG_FATAL, "ReadAndWriteData write file error");
        return false;
    }
    return true;
}

bool Entry::PayloadToDir(std::string prefixPath)
{
    std::string saveFile = "";
    auto dirPath = prefixPath.append(GetName());
    std::string estr;
    bool b = Base::TryCreateDirectory(dirPath, estr);
    if (!b) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_FATAL, "PayloadToDir mkdir failed dirPath:%s estr:%s", dirPath.c_str(), estr.c_str());
        } else {
            WRITE_LOG(LOG_FATAL, "PayloadToDir mkdir failed dirPath:%s estr:%s",
                      Hdc::MaskString(dirPath).c_str(), estr.c_str());
        }
        return false;
    }
    return true;
}

bool Entry::WriteToTar(std::ofstream &file)
{
    switch (header.FileType()) {
        case TypeFlage::ORDINARYFILE: {
            char buff[HEADER_LEN] = {0};
            header.GetBytes(reinterpret_cast<uint8_t *>(buff), HEADER_LEN);
            file.write(buff, HEADER_LEN);
            if (header.Size() == 0) {
                break;
            }
            std::string name = Base::UnicodeToUtf8(GetName().c_str(), true);
            std::ifstream inFile(name, std::ios::binary);
            if (!inFile) {
                if (Base::GetCaller() == Base::Caller::CLIENT) {
                    WRITE_LOG(LOG_FATAL, "open %s fail", name.c_str());
                } else {
                    WRITE_LOG(LOG_FATAL, "open %s fail", Hdc::MaskString(name).c_str());
                }
            }
            file << inFile.rdbuf();
            auto pading = HEADER_LEN - (needSize % HEADER_LEN);
            if (pading < HEADER_LEN) {
                char pad[HEADER_LEN] = {0};
                file.write(pad, pading);
            }
            break;
        }
        case TypeFlage::DIRECTORY: {
            char buff[HEADER_LEN] = {0};
            header.GetBytes(reinterpret_cast<uint8_t *>(buff), HEADER_LEN);
            file.write(buff, HEADER_LEN);
            break;
        }
        default:
            return false;
    }
    return true;
}
}
