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
#include <filesystem>
#include <optional>

namespace fs = std::filesystem;

namespace Hdc {

std::optional<std::string> strip_prefix(const std::string& str, const std::string& prefix)
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
    fs::path fsPath = path;
    fs::path prefixPath = prefix;
    this->prefix = prefixPath / "";
    if (fs::exists(fsPath)) {
        if (fs::is_directory(fsPath)) {
            header.UpdataFileType(TypeFlage::DIRECTORY);
            header.UpdataSize(0);
        } else if (fs::is_regular_file(fsPath)) {
            auto fileSize = fs::file_size(fsPath);
            header.UpdataSize(fileSize);
            needSize = fileSize;
            header.UpdataFileType(TypeFlage::ORDINARYFILE);
        }
    }
    UpdataName(path);
}

Entry::Entry(uint8_t data[512])
{
    header = Header(data);
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
    auto name = this->prefix / this->header.Name();
    return name.string();
}

bool Entry::UpdataName(std::string name)
{
    if (!this->prefix.string().empty()) {
        auto p_path = Hdc::strip_prefix(name, this->prefix.string());
        if (p_path.has_value()) {
            return this->header.UpdataName(p_path.value());
        }
    }
    return this->header.UpdataName(name);
}

bool Entry::SaveToFile(std::string prefixPath)
{
    if (!IsFinish()) {
        return false;
    }

    switch (this->header.FileType()) {
        case TypeFlage::ORDINARYFILE: {
            auto saveFile = prefixPath.append(GetName());
            std::ofstream file(saveFile, std::ios::out | std::ios::binary);
            if (!file.is_open()) {
                WRITE_LOG(LOG_FATAL, "open %s fail", saveFile.c_str());
                return false;
            }
            WRITE_LOG(LOG_INFO, "saveFile %s, size %ld", saveFile.c_str(), this->data.size());
            file.write((const char*)this->data.data(), this->data.size());
            file.close();
            if (file.fail()) {
                return false;
            }
            break;
        }
        case TypeFlage::DIRECTORY: {
            auto dirPath = prefixPath.append(GetName());
            fs::create_directory(dirPath);
        }
        default:
            return false;
    }
    return true;
}

bool Entry::WriteToTar(std::ofstream &file)
{
    switch (header.FileType()) {
        case TypeFlage::ORDINARYFILE: {
            char buff[HEADER_LEN] = {0};
            header.GetBytes((uint8_t*)buff);
            file.write(buff, HEADER_LEN);
            std::ifstream inFile(GetName(), std::ios::binary);
            file << inFile.rdbuf();
            auto pading = HEADER_LEN - (needSize % HEADER_LEN);
            if (pading < HEADER_LEN) {
                WRITE_LOG(LOG_INFO, "pading %ld", pading);
                char pad[HEADER_LEN] = {0};
                file.write(pad, pading);
            }
            break;
        }
        case TypeFlage::DIRECTORY: {
            char buff[HEADER_LEN] = {0};
            header.GetBytes((uint8_t*)buff);
            file.write(buff, HEADER_LEN);
            break;
        }
        default:
            return false;
    }
    return true;
}
}
