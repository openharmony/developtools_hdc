/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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
#ifndef HDC_BINARY_TLV_H
#define HDC_BINARY_TLV_H
#include <cstdint>
#include <cstddef>

constexpr uint32_t TLV_HEAD_SIZE = sizeof(uint32_t) + sizeof(uint32_t);
constexpr uint32_t TLV_VALUE_MAX = 16 * 1024 * 1024;

extern "C" {
void *TlvCreate();
void TlvDestroy(void *buf);
bool TlvAppend(void *buf, uint32_t tag, const uint8_t *val, uint32_t len);
bool TlvAppendStr(void *buf, uint32_t tag, const char *val);
uint32_t TlvGetSize(const void *buf);
bool TlvCopyToBuf(const void *buf, uint8_t *dst, uint32_t size);
bool TlvFind(const void *buf, uint32_t tag, uint32_t *len, uint8_t *val, uint32_t maxLen);
void TlvClear(void *buf);
bool TlvContainInvalid(const void *buf);
}

#endif
