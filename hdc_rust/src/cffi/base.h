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
#ifndef RUST_HDC_BASE
#define RUST_HDC_BASE

#include <cstdio>
#include <securec.h>
#include <vector>
#include <string>

namespace Hdc {
using namespace std;

constexpr uint16_t MAX_SIZE_IOBUF = 61440;
constexpr uint16_t BUF_SIZE_MEDIUM = 512;
constexpr uint16_t BUF_SIZE_DEFAULT = 1024;
constexpr uint16_t BUF_SIZE_DEFAULT4 = BUF_SIZE_DEFAULT * 4;

inline int GetMaxBufSize()
{
    return MAX_SIZE_IOBUF;
}

template<class T> int ZeroStruct(T &structBuf)
{
    return memset_s(&structBuf, sizeof(T), 0, sizeof(T));
}

const std::string StringFormat(const char * const formater, va_list &vaArgs);

const std::string StringFormat(const char * const formater, ...);

bool RunPipeComand(const char *cmdString, char *outBuf, uint16_t sizeOutBuf, bool ignoreTailLf);
}
#endif