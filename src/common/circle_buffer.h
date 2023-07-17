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

#ifndef CIRCLE_BUFFER_H_
#define CIRCLE_BUFFER_H_

#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <securec.h>
#include <thread>
#include <list>

namespace Hdc {
constexpr uint64_t CIRCLE_SIZE = 16;
constexpr uint64_t BUF_SIZE = 62464; // MAX_USBFFS_BULK

class CircleBuffer {
public:
    CircleBuffer();
    ~CircleBuffer();
    uint8_t *Malloc();
    void Free();

private:
    bool Full() const;
    bool Empty() const;
    void Init();
    bool FirstMalloc();
    uint64_t head_;
    uint64_t tail_;
    uint64_t size_;
    std::mutex mutex_;
    std::list<uint8_t *> buffers_;
    bool run_;
    bool mallocInit_;
    std::thread thread_;
    std::mutex timerMutex_;
    std::condition_variable timerCv_;
    std::chrono::steady_clock::time_point begin_;
    static void Timer(void *object);
    void DecreaseMemory();
    void FreeMemory();
    void TimerNotify();
    void TimerSleep();
    void TimerStart();
    void TimerStop();
    int64_t Interval();
};
}

#endif

