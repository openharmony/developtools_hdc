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
#include "ctimer.h"

void CTimer::Start(unsigned int imsec, bool immediatelyRun)
{
    if (imsec == 0 || imsec == static_cast<unsigned int>(-1)) {
        return;
    }
    exit.store(false);
    msec = imsec;
    this->immediatelyRun.store(immediatelyRun);
    thread = std::thread(std::bind(&CTimer::Run, this));
}
 
void CTimer::Stop()
{
    exit.store(true);
    cond.notify_all();
    if (thread.joinable()) {
        thread.join();
    }
}

void CTimer::SetExit(bool exit)
{
    this->exit.store(exit);
}

void CTimer::Run()
{
    if (immediatelyRun.load()) {
        if (func) {
            func();
        }
    }

    while (!exit.load()) {
        {
            std::unique_lock<std::mutex> locker(mutex);
            cond.wait_for(locker, std::chrono::milliseconds(msec), [this]() { return exit.load(); });
        }

        if (exit.load()) {
            return;
        }

        if (func) {
            func();
        }
    }
}