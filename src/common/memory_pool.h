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

#ifndef MEMORY_POOL_H_
#define MEMORY_POOL_H_

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Hdc {

class MemoryPool {
public:
#ifdef MEMORY_POOL_ENABLE
    void* Allocate(size_t size);
    void Deallocate(void* ptr);

    static MemoryPool& Instance();
#else
    void* Allocate(size_t size)
    {
        return std::malloc(size);
    }
    void Deallocate(void* ptr)
    {
        std::free(ptr);
    }

    static MemoryPool& Instance()
    {
        static MemoryPool memoryPool;
        return memoryPool;
    }
#endif

private:
    struct MemoryBlock {
        void* memory;
        size_t size;
        std::chrono::steady_clock::time_point lastFreeTime;

        MemoryBlock(void* mem, size_t sz)
            : memory(mem), size(sz), lastFreeTime(std::chrono::steady_clock::now()) {}

        bool operator<(const MemoryBlock& other) const
        {
            return memory < other.memory;
        }
    };

    std::unordered_map<size_t, std::list<MemoryBlock>> freeMemoryMap_;
    std::unordered_map<void*, MemoryBlock> usedMemoryMap_;
    std::mutex mutex_;

    std::atomic<bool> cleanupThreadRunning_ = true;
    [[maybe_unused]] std::condition_variable cleanupCv_;
    std::mutex cleanupMutex_;
    std::thread cleanupThread_;
    void CleanupWorker();
    void Cleanup();

#ifdef MEMORY_POOL_ENABLE
    MemoryPool();
    ~MemoryPool();
#else
    MemoryPool() = default;
    ~MemoryPool() = default;
#endif

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool(const MemoryPool&&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&&) = delete;
};

}   // namespace Hdc

#endif
