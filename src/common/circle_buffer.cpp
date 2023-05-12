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

namespace Hdc {
CircleBuffer::CircleBuffer()
{
    head = 0;
    tail = 0;
    size = CIRCLE_SIZE;
    for (int i = 0; i < size; i++) {
        uint8_t *buf = new(std::nothrow) uint8_t[BUF_SIZE];
        buffers.push_back(buf);
    }
    TimerStart();
}

CircleBuffer::~CircleBuffer()
{
    TimerStop();
    for (int i = 0; i < size; i++) {
        delete[] buffers[i];
    }
}

bool CircleBuffer::Full()
{
    return (tail + 1) % size == head;
}

bool CircleBuffer::Empty()
{
    return tail == head;
}

uint8_t *CircleBuffer::Malloc()
{
    std::unique_lock<std::mutex> lock(mutex);
    uint8_t *buf = nullptr;
    if (Full()) {
        buf = new(std::nothrow) uint8_t[BUF_SIZE];
        if (buf == nullptr) {
            return nullptr;
        }
        buffers.insert(buffers.begin() + tail, buf);
        size++;
        if (head > tail) {
            head = (head + 1) % size;
        }
    } else {
        buf = buffers[tail];
    }
    (void)memset_s(buf, BUF_SIZE, 0, BUF_SIZE);
    tail = (tail + 1) % size;
    begin = std::chrono::steady_clock::now();
    return buf;
}

void CircleBuffer::Free()
{
    std::unique_lock<std::mutex> lock(mutex);
    if (Empty()) {
        return;
    }
    head = (head + 1) % size;
    begin = std::chrono::steady_clock::now();
}

void CircleBuffer::FreeMemory()
{
    std::unique_lock<std::mutex> lock(mutex);
    int64_t freeTime = 10;
    int64_t val = Interval();
    if (val <= freeTime) {
        return;
    }
    int left = size - CIRCLE_SIZE;
    if (left < 1) {
        return;
    }
    bool b = Empty();
    if (b) {
        for (int i = left; i > 0; i--) {
            delete[] buffers[i - 1 + CIRCLE_SIZE];
            buffers.pop_back();
        }
        head = 0;
        tail = 0;
        size = CIRCLE_SIZE;
    }
}

void CircleBuffer::Timer(void *object)
{
    CircleBuffer *cirbuf = (CircleBuffer *)object;
    while (cirbuf->run) {
        cirbuf->FreeMemory();
        cirbuf->TimerSleep();
    }
}

void CircleBuffer::TimerStart()
{
    run = true;
    begin = std::chrono::steady_clock::now();
    thread = std::thread (Timer, this);
}

void CircleBuffer::TimerStop()
{
    run = false;
    TimerNotify();
    thread.join();
}

void CircleBuffer::TimerSleep()
{
    std::unique_lock<std::mutex> lock(timerMutex);
    timerCv.wait_for(lock, std::chrono::seconds(1));
}

void CircleBuffer::TimerNotify()
{
    std::unique_lock<std::mutex> lock(timerMutex);
    timerCv.notify_one();
}

int64_t CircleBuffer::Interval()
{
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - begin);
    return duration.count();
}
}

