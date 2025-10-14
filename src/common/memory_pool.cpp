/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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

#include "memory_pool.h"

#ifdef MEMORY_POOL_ENABLE
namespace Hdc {
void* MemoryPool::Allocate(size_t size)
{
    if (size == 0) {
        return nullptr;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    auto it = freeMemoryMap_.find(size);
    if (it != freeMemoryMap_.end() && !it->second.empty()) {
        auto& blockList = it->second;
        MemoryBlock block = std::move(blockList.back());
        blockList.pop_back();

        void* ret = block.memory;
        usedMemoryMap_.emplace(block.memory, std::move(block));

        if (blockList.empty()) {
            freeMemoryMap_.erase(it);
        }

        return ret;
    }

    void* newMemory = std::malloc(size);
    if (!newMemory) {
        return nullptr;
    }

    usedMemoryMap_.emplace(newMemory, MemoryBlock(newMemory, size));

    return newMemory;
}

void MemoryPool::Deallocate(void* ptr)
{
    if (ptr == nullptr) {
        return;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    auto it = usedMemoryMap_.find(ptr);
    if (it == usedMemoryMap_.end()) {
        return;
    }

    MemoryBlock block = it->second;
    usedMemoryMap_.erase(it);
    block.lastFreeTime = std::chrono::steady_clock::now();
    freeMemoryMap_[block.size].push_back(std::move(block));
}

MemoryPool& MemoryPool::Instance()
{
    static MemoryPool memoryPool;
    return memoryPool;
}

MemoryPool::MemoryPool()
{
    cleanupThread_ = std::thread(&MemoryPool::CleanupWorker, this);
}

MemoryPool::~MemoryPool()
{
    cleanupThreadRunning_ = false;
    cleanupCv_.notify_one();
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
}

void MemoryPool::CleanupWorker()
{
    static const std::chrono::seconds cleanupInterval = std::chrono::seconds(200);
    while (cleanupThreadRunning_.load()) {
        std::unique_lock<std::mutex> lock(cleanupMutex_);

        if (cleanupCv_.wait_for(lock, cleanupInterval, [this]() {
            return !cleanupThreadRunning_.load();
        })) {
            break;
        }

        if (cleanupThreadRunning_.load()) {
            Cleanup();
        }
    }
}

void MemoryPool::Cleanup()
{
    static const std::chrono::minutes threshold = std::chrono::minutes(3);
    std::chrono::steady_clock::time_point time = std::chrono::steady_clock::now() - threshold;

    std::unique_lock<std::mutex> lock(mutex_);
    for (auto it = freeMemoryMap_.begin(); it != freeMemoryMap_.end();) {
        auto& [size, blockList] = *it;

        for (auto block = blockList.begin(); block != blockList.end();) {
            if (block->lastFreeTime < time) {
                std::free(block->memory);
                block = blockList.erase(block);
            } else {
                break;
            }
        }

        if (blockList.empty()) {
            it = freeMemoryMap_.erase(it);
        } else {
            ++it;
        }
    }
}
}   // namespace Hdc
#endif  // MEMORY_POOL_ENABLE
