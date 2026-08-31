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
#include "memory_pool_wrapper.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

namespace {
constexpr bool POOL_ENABLE = false;
constexpr uint32_t MAX_ALLOC_SIZE = 16 * 1024 * 1024;
std::mutex g_mutex;
std::map<uint32_t, std::vector<void *>> g_freeMap;
std::map<void *, uint32_t> g_usedMap;

bool IsValidAllocSize(uint32_t size)
{
    return size > 0 && size <= MAX_ALLOC_SIZE;
}

void *SafeMalloc(uint32_t size)
{
    if (size == 0 || size > MAX_ALLOC_SIZE) {
        return nullptr;
    }
    return malloc(size);
}

void *PoolAllocate(uint32_t size)
{
    if (!IsValidAllocSize(size)) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_freeMap.find(size);
    if (it != g_freeMap.end() && !it->second.empty()) {
        void *ptr = it->second.back();
        it->second.pop_back();
        g_usedMap[ptr] = size;
        return ptr;
    }
    void *ptr = SafeMalloc(size);
    if (ptr != nullptr) {
        g_usedMap[ptr] = size;
    }
    return ptr;
}

void PoolDeallocate(void *ptr)
{
    if (ptr == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_usedMap.find(ptr);
    if (it == g_usedMap.end()) {
        free(ptr);
        return;
    }
    uint32_t size = it->second;
    g_usedMap.erase(it);
    g_freeMap[size].push_back(ptr);
}

void PoolCleanup()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto &pair : g_freeMap) {
        for (void *ptr : pair.second) {
            free(ptr);
        }
    }
    g_freeMap.clear();
}
}

extern "C" {
void *MemPoolAllocate(uint32_t size)
{
    if (!IsValidAllocSize(size)) {
        return nullptr;
    }
    if (POOL_ENABLE) {
        return PoolAllocate(size);
    }
    return SafeMalloc(size);
}

void MemPoolDeallocate(void *ptr)
{
    if (ptr == nullptr) {
        return;
    }
    if (POOL_ENABLE) {
        PoolDeallocate(ptr);
    } else {
        free(ptr);
    }
}

void MemPoolCleanup()
{
    if (POOL_ENABLE) {
        PoolCleanup();
    }
}

uint32_t MemPoolFreeCount()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    uint32_t count = 0;
    for (const auto &pair : g_freeMap) {
        count += static_cast<uint32_t>(pair.second.size());
    }
    return count;
}
}
