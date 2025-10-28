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

#include "header.h"

namespace Hdc {
class Entry {
public:
    Entry(std::string prefix, std::string path);
    Entry(uint8_t data[512], int dataLen);
    ~Entry() {}

    bool IsFinish()
    {
        return this->needSize == 0;
    }

    bool IsInvalid()
    {
        return this->header.IsInvalid();
    }

    void AddData(uint8_t *data, size_t len);
    uint64_t Size()
    {
        return header.Size();
    }

    bool CopyPayload(std::string prefixPath, std::ifstream &inFile);
    bool PayloadToFile(std::string prefixPath, std::ifstream &inFile);
    bool PayloadToDir(std::string prefixPath);
    bool ReadAndWriteData(std::ifstream &inFile, std::ofstream &outFile, uint8_t *buffAppend,
        int readSize, int writeSize);
    bool WriteToTar(std::ofstream &file);

    std::string GetName();
    bool UpdataName(std::string name);

private:
    Header header;
    uint64_t needSize;
    std::string prefix;
    std::vector<uint8_t> data;
};

}
#endif
