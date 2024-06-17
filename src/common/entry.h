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
#ifndef HDC_ENTRY_H
#define HDC_ENTRY_H

#include <vector>
#include <filesystem>

#include "header.h"

namespace fs = std::filesystem;

namespace Hdc {
class Entry {
public:
    Entry(std::string prefix, std::string path);
    explicit Entry(uint8_t data[512]);
    ~Entry() {}

    bool IsFinish()
    {
        return this->need_size == 0;
    }

    bool IsInvalid()
    {
        return this->header.IsInvalid();
    }

    void AddData(uint8_t *data, size_t len);
    size_t Size()
    {
        return header.Size();
    }

    bool SaveToFile(std::string prefixPath);
    bool WriteToTar(std::ofstream &file);

    std::string GetName();
    bool UpdataName(std::string name);

private:
    Header header;
    size_t need_size;
    fs::path prefix;
    std::vector<uint8_t> data;
};

}
#endif
