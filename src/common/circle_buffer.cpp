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
    run = true;
    begin = std::chrono::steady_clock::now();
    thread = std::thread (Timer, this);
}

CircleBuffer::~CircleBuffer()
{
    run = false;
    thread.join();
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
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - begin);
    int left = size - CIRCLE_SIZE;
    bool b = Empty();
    int freeTime = 10;
    if (b && left > 0 && duration.count() > freeTime) {
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
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
}

