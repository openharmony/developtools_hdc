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
#include <mutex>

using namespace std;

namespace Hdc {
    #define US_PER_SEC (1000 * 1000)
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
    LoopStatus::LoopStatus(uv_loop_t *loop, const string &loopName) : mLoop(loop), mLoopName(loopName)
    {
        mCallBackTime.tv_sec = 0;
        mCallBackTime.tv_usec = 0;

        std::unique_lock<std::mutex> lock(g_mapLoopStatusMutex);
        if (g_loopStatusMap.count(mLoop)) {
            return;
        }
        g_loopStatusMap[loop] = this;
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
        mCallBackTime.tv_sec = 0;
        mCallBackTime.tv_usec = 0;
        mHandleName = "idle";
    }
    bool LoopStatus::Busy(void) const
    {
        return (mCallBackTime.tv_sec != 0 || mCallBackTime.tv_usec != 0);
    }
    void LoopStatus::Display(const string &info) const
    {
        if (Busy()) {
            WRITE_LOG(LOG_FATAL, "%s loop[%s] is busy for [%s] start[%llu] duration[%llu]",
                      info.c_str(), mLoopName.c_str(), mHandleName.c_str(),
                      TimeUsec(mCallBackTime), TimeSub(TimeNow(), mCallBackTime));
        } else {
            WRITE_LOG(LOG_FATAL, "%s loop[%s] is idle", info.c_str(), mLoopName.c_str());
        }
    }

} /* namespace Hdc  */
