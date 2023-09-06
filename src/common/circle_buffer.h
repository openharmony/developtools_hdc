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
#include <map>
#include <mutex>
#include <new>
#include <securec.h>
#include <thread>

namespace Hdc {
constexpr uint64_t BUF_SIZE = 62464; // MAX_USBFFS_BULK

struct Data {
    uint8_t *buf;
    bool used;
    std::chrono::steady_clock::time_point begin;
};

class CircleBuffer {
public:
    CircleBuffer();
    ~CircleBuffer();
    uint8_t *Malloc();
    void Free(const uint8_t *buf);

private:
    std::mutex mutex_;
    std::map<uint64_t, Data *> buffers_;
    bool run_;
    std::thread thread_;
    std::mutex timerMutex_;
    std::condition_variable timerCv_;
    static void Timer(void *object);
    void FreeMemory();
    void TimerNotify();
    void TimerSleep();
    void TimerStart();
    void TimerStop();
};
}

#endif

