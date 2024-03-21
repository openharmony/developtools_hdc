/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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
#include <lib/lz4.h>

namespace Hdc {

extern "C" int LZ4CompressTransfer(const char* data, char* dataCompress,
                                   int data_size, int compressCapacity)
{
    return LZ4_compress_default(reinterpret_cast<const char*>(data), reinterpret_cast<char*>(dataCompress),
                                data_size, compressCapacity);
}

extern "C" int LZ4DeompressTransfer(const char* data, char* dataDecompress,
                                    int data_size, int decompressCapacity)
{
    return LZ4_decompress_safe(reinterpret_cast<const char*>(data), reinterpret_cast<char*>(dataDecompress),
                               data_size, decompressCapacity);
}

}