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

#include "log.h"
#include "uv_status.h"
#include <sys/time.h>
#include <thread>
#include <mutex>

using namespace std;

namespace Hdc {
    static int64_t TimeSub(const struct timeval end, const struct timeval start)
    {
        int64_t endUS = end.tv_sec * US_PER_SEC + end.tv_usec;
        int64_t startUS = start.tv_sec * US_PER_SEC + start.tv_usec;

        return endUS - startUS;
    }
    static uint64_t TimeUsec(const struct timeval time)
    {
        return time.tv_sec * US_PER_SEC + time.tv_usec;
    }
    static struct timeval TimeNow(void)
    {
        struct timeval now;
        gettimeofday(&now, nullptr);
        return now;
    }

    static std::map<uv_loop_t *, LoopStatus *> g_loopStatusMap;
    static std::mutex g_mapLoopStatusMutex;
    void DispAllLoopStatus(const string &info)
    {
        std::unique_lock<std::mutex> lock(g_mapLoopStatusMutex);
        for (auto [loop, stat] : g_loopStatusMap) {
            stat->Display(info);
        }
    }
    void LoopStatus::ReportTimerProc(uv_timer_t *req)
    {
        LoopStatus *ls = reinterpret_cast<LoopStatus *>(req->data);
        CALLSTAT_GUARD(*ls, req->loop, "LoopStatus::ReportTimerProc");
    }
    void LoopStatus::StartReportTimer(void)
    {
        uv_timer_t timer;
        uv_timer_init(mLoop, &timer);
        timer.data = this;
        uv_timer_start(&timer, ReportTimerProc, 0, 1 * MS_PER_SEC);
    }
    LoopStatus::LoopStatus(uv_loop_t *loop, const string &loopName) : mLoop(loop), mLoopName(loopName)
    {
        mBusyNow = false;
        mCallBackTime.tv_sec = 0;
        mCallBackTime.tv_usec = 0;

        std::unique_lock<std::mutex> lock(g_mapLoopStatusMutex);
        if (g_loopStatusMap.count(mLoop)) {
            return;
        }
        g_loopStatusMap[mLoop] = this;
    }
    LoopStatus::~LoopStatus()
    {
        std::unique_lock<std::mutex> lock(g_mapLoopStatusMutex);
        if (!g_loopStatusMap.count(mLoop)) {
            return;
        }
        g_loopStatusMap.erase(mLoop);
    }
    void LoopStatus::HandleStart(const uv_loop_t *loop, const string &handle)
    {
        if (loop != mLoop) {
            WRITE_LOG(LOG_FATAL, "not match loop [%s] for run [%s]", mLoopName.c_str(), handle.c_str());
            return;
        }
        if (Busy()) {
            WRITE_LOG(LOG_FATAL, "the loop is busy for [%s] cannt run [%s]", mHandleName.c_str(), handle.c_str());
            return;
        }
        mBusyNow = true;
        gettimeofday(&mCallBackTime, nullptr);
        mHandleName = handle;
    }
    void LoopStatus::HandleEnd(const uv_loop_t *loop)
    {
        if (loop != mLoop) {
            WRITE_LOG(LOG_FATAL, "not match loop [%s] for end [%s]", mLoopName.c_str(), mHandleName.c_str());
            return;
        }
        if (!Busy()) {
            WRITE_LOG(LOG_FATAL, "the loop [%s] is idle now", mLoopName.c_str());
            return;
        }
        mBusyNow = false;
    }
    bool LoopStatus::Busy(void) const
    {
        return mBusyNow;
    }
    void LoopStatus::Display(const string &info) const
    {
        if (Busy()) {
            WRITE_LOG(LOG_FATAL, "%s loop[%s] is busy for [%s] start[%llu] duration[%llu]",
                      info.c_str(), mLoopName.c_str(), mHandleName.c_str(),
                      TimeUsec(mCallBackTime), TimeSub(TimeNow(), mCallBackTime));
        } else {
            WRITE_LOG(LOG_INFO, "%s loop[%s] is idle", info.c_str(), mLoopName.c_str());
        }
    }
    void LoopStatus::HungCheck(int64_t timeout) const
    {
        if (TimeSub(TimeNow(), mCallBackTime) > timeout) {
            Display("hung :");
        }
    }

    int GetLoopHungTimeout(void)
    {
        return US_PER_SEC / 2;
    }
    int GetLoopMonitorPeriod(void)
    {
        return 3 * MS_PER_SEC;
    }
    static void LoopMonitorWorker(void)
    {
        WRITE_LOG(LOG_FATAL, "LoopMonitorWorker every %d ms check hung %d us",
                  GetLoopMonitorPeriod(), GetLoopHungTimeout());
        bool exit = false;
        while (!exit) {
            // every 1 second check one time
            uv_sleep(GetLoopMonitorPeriod());
            std::unique_lock<std::mutex> lock(g_mapLoopStatusMutex);
            if (g_loopStatusMap.empty()) {
                WRITE_LOG(LOG_FATAL, "LoopMonitorWorker the loop status map is empty, stop monitor");
                exit = true;
            }
            for (auto [loop, stat] : g_loopStatusMap) {
                stat->HungCheck(GetLoopHungTimeout());
            }
        }
    }
    void StartLoopMonitor(void)
    {
        static std::once_flag flag;
        std::call_once(flag, [&]() {
            std::thread t(LoopMonitorWorker);
            t.detach();
        });
    }
} /* namespace Hdc  */
