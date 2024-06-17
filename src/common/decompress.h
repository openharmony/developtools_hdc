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
#ifndef __DECOMPRESS_H__
#define __DECOMPRESS_H__

#include <vector>
#include <string>

#include "entry.h"

namespace Hdc {
class Decompress
{
public:
    Decompress(std::string tarPath)
        : tarPath(tarPath){}
    ~Decompress() {}

    bool DecompressToLocal(std::string decPath);

private:
    /* bool ReadData(); */

    std::vector<Entry> entrys;
    std::string tarPath;
};

}
#endif
