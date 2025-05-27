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

#include "circle_buffer.h"
#include "base.h"
#ifdef CONFIG_USE_JEMALLOC_DFX_INIF
#include <malloc.h>
#endif

namespace Hdc {
CircleBuffer::CircleBuffer()
{
    run_ = false;
    TimerStart();
#ifdef CONFIG_USE_JEMALLOC_DFX_INIF
    mallopt(M_DELAYED_FREE, M_DELAYED_FREE_DISABLE);
    mallopt(M_SET_THREAD_CACHE, M_THREAD_CACHE_DISABLE);
#endif
}

CircleBuffer::~CircleBuffer()
{
    TimerStop();
    for (auto iter = buffers_.begin(); iter != buffers_.end();) {
        Data *data = iter->second;
        delete[] data->buf;
        delete data;
        iter = buffers_.erase(iter);
    }
}

uint8_t *CircleBuffer::Malloc()
{
    const size_t bufSize = static_cast<size_t>(Base::GetUsbffsBulkSize());
    uint8_t *buf = nullptr;
    std::unique_lock<std::mutex> lock(mutex_);
    for (auto iter = buffers_.begin(); iter != buffers_.end(); ++iter) {
        Data *data = iter->second;
        if (data->used == false) {
            data->used = true;
            data->begin = std::chrono::steady_clock::now();
            buf = data->buf;
            break;
        }
    }
    if (buf == nullptr) {
        Data *data = new(std::nothrow) Data();
        if (data == nullptr) {
            return nullptr;
        }
        data->used = true;
        data->begin = std::chrono::steady_clock::now();
        data->buf = new(std::nothrow) uint8_t[bufSize];
        if (data->buf == nullptr) {
            delete data;
            return nullptr;
        }
        uint64_t key = reinterpret_cast<uint64_t>(data->buf);
        buffers_[key] = data;
        buf = data->buf;
    }
    (void)memset_s(buf, bufSize, 0, bufSize);
    return buf;
}

void CircleBuffer::Free(const uint8_t *buf)
{
    std::unique_lock<std::mutex> lock(mutex_);
    uint64_t key = reinterpret_cast<uint64_t>(buf);
    auto findData = buffers_.find(key);
    if (findData == buffers_.end()) {
        WRITE_LOG(LOG_FATAL, "Free data not found.");
        return;
    }
    Data *data = findData->second;
    if (data != nullptr) {
        data->used = false;
        data->begin = std::chrono::steady_clock::now();
    } else {
        WRITE_LOG(LOG_FATAL, "Free data is nullptr.");
    }
}

void CircleBuffer::FreeMemory()
{
    std::unique_lock<std::mutex> lock(mutex_);
    constexpr int64_t decreaseTime = 5; // 5s
    auto end = std::chrono::steady_clock::now();
    for (auto iter = buffers_.begin(); iter != buffers_.end();) {
        bool remove = false;
        Data *data = iter->second;
        if (data->used == false) {
            auto begin = data->begin;
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - begin);
            if (duration.count() > decreaseTime) {
                remove = true;
            }
        }
        if (remove) {
            delete[] data->buf;
            delete data;
            iter = buffers_.erase(iter);
        } else {
            ++iter;
        }
    }
}

void CircleBuffer::Timer(void *object)
{
#ifdef CONFIG_USE_JEMALLOC_DFX_INIF
    mallopt(M_DELAYED_FREE, M_DELAYED_FREE_DISABLE);
    mallopt(M_SET_THREAD_CACHE, M_THREAD_CACHE_DISABLE);
#endif
    CircleBuffer *cirbuf = reinterpret_cast<CircleBuffer *>(object);
    while (cirbuf->run_) {
        cirbuf->FreeMemory();
        cirbuf->TimerSleep();
    }
}

void CircleBuffer::TimerStart()
{
#ifndef HDC_HOST
    if (!run_) {
        run_ = true;
        thread_ = std::thread([this]() { Timer(this); });
    }
#endif
}

void CircleBuffer::TimerStop()
{
#ifndef HDC_HOST
    if (run_) {
        run_ = false;
        TimerNotify();
        thread_.join();
    }
#endif
}

void CircleBuffer::TimerSleep()
{
    std::unique_lock<std::mutex> lock(timerMutex_);
    timerCv_.wait_for(lock, std::chrono::seconds(1));
}

void CircleBuffer::TimerNotify()
{
    std::unique_lock<std::mutex> lock(timerMutex_);
    timerCv_.notify_one();
}
}
