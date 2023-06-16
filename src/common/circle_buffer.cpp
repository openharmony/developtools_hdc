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

namespace Hdc {
CircleBuffer::CircleBuffer()
{
    Init();
    run_ = false;
    begin_ = std::chrono::steady_clock::now();
}

CircleBuffer::~CircleBuffer()
{
    TimerStop();
    for (size_t i = 0; i < buffers_.size(); i++) {
        delete[] buffers_[i];
    }
}

void CircleBuffer::Init()
{
    head_ = 0;
    tail_ = 0;
    const int firstSize = 2;
    size_ = firstSize;
    mallocInit_ = false;
}

bool CircleBuffer::Full() const
{
    return (tail_ + 1) % size_ == head_;
}

bool CircleBuffer::Empty() const
{
    return tail_ == head_;
}

bool CircleBuffer::FirstMalloc()
{
    if (mallocInit_) {
        return true;
    }
    const int firstSize = 2;
    for (uint64_t i = 0; i < firstSize; i++) {
        uint8_t *buf = new(std::nothrow) uint8_t[BUF_SIZE];
        if (buf == nullptr) {
            continue;
        }
        buffers_.push_back(buf);
    }
    if (buffers_.empty()) {
        return false;
    }

    TimerStart();
    mallocInit_ = true;
    size_ = static_cast<uint64_t>(buffers_.size());
    return true;
}

uint8_t *CircleBuffer::Malloc()
{
    const size_t bufSize = Base::GetUsbffsBulkSize();
    std::unique_lock<std::mutex> lock(mutex_);
    if (!FirstMalloc()) {
        return nullptr;
    }
    uint8_t *buf = nullptr;
    if (Full()) {
        for (int i = 0; i < CIRCLE_SIZE; i++) {
            buf = new(std::nothrow) uint8_t[bufSize];
            if (buf == nullptr) {
                return nullptr;
            }
            buffers_.insert(buffers_.begin() + tail_, buf);
            size_++;
            if (head_ > tail_) {
                head_ = (head_ + 1) % size_;
            }
        }
    }
    buf = buffers_[tail_];
    (void)memset_s(buf, bufSize, 0, bufSize);
    tail_ = (tail_ + 1) % size_;
    begin_ = std::chrono::steady_clock::now();
    return buf;
}

void CircleBuffer::Free()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (Empty()) {
        return;
    }
    head_ = (head_ + 1) % size_;
    begin_ = std::chrono::steady_clock::now();
}

void CircleBuffer::FreeMemory()
{
    std::unique_lock<std::mutex> lock(mutex_);
    int64_t freeTime = 30; // 30s
    if (!Empty() || Interval() < freeTime) {
        return;
    }

    size_t bufferSize = buffers_.size();
    for (size_t i = bufferSize; i > 0; i--) {
        delete[] buffers_[i - 1];
        buffers_.pop_back();
    }
    Init();
}

void CircleBuffer::Timer(void *object)
{
    CircleBuffer *cirbuf = (CircleBuffer *)object;
    while (cirbuf->run_) {
        cirbuf->FreeMemory();
        cirbuf->TimerSleep();
    }
}

void CircleBuffer::TimerStart()
{
    if (!run_) {
        run_ = true;
        begin_ = std::chrono::steady_clock::now();
        thread_ = std::thread(Timer, this);
    }
}

void CircleBuffer::TimerStop()
{
    if (run_) {
        run_ = false;
        TimerNotify();
        thread_.join();
    }
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

int64_t CircleBuffer::Interval()
{
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - begin_);
    return duration.count();
}
}
