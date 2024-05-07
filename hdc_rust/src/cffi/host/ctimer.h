/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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
#ifndef HDC_RUST_CTIMER_H
#define HDC_RUST_CTIMER_H
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <functional>
 
class CTimer {
public:
    template<class F>
    explicit CTimer(F func)
    {
        this->func = func;
    }
    virtual ~CTimer() {}
    void Start(unsigned int msec, bool immediatelyRun = false);
    void Stop();
    void SetExit(bool exit);
private:
    void Run();
private:
    std::atomic_bool exit = false;
    std::atomic_bool immediatelyRun = false;
    unsigned int msec = 1000;
    std::function<void(void)> func;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cond;
};
#endif
