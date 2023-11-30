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
#include <lz4.h>

namespace Hdc {

extern "C" int LZ4_compress_transfer(const char* data, char* data_compress, int data_size, int compress_capacity) {
    return LZ4_compress_default((const char *)data, (char *)data_compress,
                                                    data_size, compress_capacity);
}

extern "C" int LZ4_decompress_transfer(const char* data, char* data_decompress, int data_size, int decompress_capacity) {

    return LZ4_decompress_safe((const char *)data, (char *)data_decompress,
                                                    data_size, decompress_capacity);
}

}