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

#include <filesystem>
#include <sstream>
#include <fstream>
#include <optional>
#include <iostream>

namespace fs = std::filesystem;

namespace Hdc {

bool Decompress::DecompressToLocal(std::string decPath)
{
    if (!fs::exists(tarPath) || !fs::is_regular_file(tarPath)) {
        WRITE_LOG(LOG_FATAL, "%s not exist, or not file", tarPath.c_str());
        return false;
    }

    auto fileSize = fs::file_size(tarPath);
    if (fileSize == 0 || fileSize % HEADER_LEN != 0) {
        WRITE_LOG(LOG_FATAL, "file is not tar %s", tarPath.c_str());
        return false;
    }

    if (fs::exists(decPath)) {
        if (fs::is_regular_file(decPath)) {
            WRITE_LOG(LOG_FATAL, "path is exist, and path not dir %s", decPath.c_str());
            return false;
        }
    } else {
        fs::create_directories(decPath);
    }

    uint8_t buff[HEADER_LEN];
    std::ifstream inFile(tarPath);

    std::optional<std::ofstream> outFile = std::nullopt;
    std::optional<Entry> entry = std::nullopt;
    while(1) {
        inFile.read(reinterpret_cast<char*>(buff), HEADER_LEN);
        auto readcnt = inFile.gcount();
        if (readcnt == 0) {
            WRITE_LOG(LOG_INFO, "read EOF");
            break;
        }
        if (inFile.fail() || readcnt != HEADER_LEN) {
            WRITE_LOG(LOG_FATAL, "read file error");
            break;
        }
        if (!entry.has_value()) {
            WRITE_LOG(LOG_INFO, "new entry =================>");
            entry = Entry(buff);
            if (entry.value().IsFinish()) {
                entry.value().SaveToFile(decPath);
                entry = std::nullopt;
            }
            continue;
        }
        entry.value().AddData(buff, HEADER_LEN);
        if (entry.value().IsFinish()) {
            entry.value().SaveToFile(decPath);
            entry = std::nullopt;
        }
    }
    if (outFile.has_value()) {
        outFile.value().close();
    }
    inFile.close();
    return true;
}
}

