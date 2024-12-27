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

#include "uv_status.h"
#include <sys/time.h>

using namespace std;

namespace Hdc
{

    static int64_t time_sub(const struct timeval end, const struct timeval start)
    {
        int64_t endUS = end.tv_sec * 1000 * 1000 + end.tv_usec;
        int64_t startUS = start.tv_sec * 1000 * 1000 + start.tv_usec;

        return endUS - startUS;
    }
    static uint64_t time_ns(const struct timeval time)
    {
        return time.tv_sec * 1000 * 1000 + time.tv_usec;
    }

    void HandleStatus::Init(const string &name)
    {
        mName = name;
        mState = HANDLE_NONE;
        mInTime.tv_sec = 0;
        mInTime.tv_usec = 0;
        mOutTime.tv_sec = 0;
        mOutTime.tv_usec = 0;
        mTotalDuration = 0;
        mDealedBytes = 0;
    }
    HandleStatus::HandleStatus()
    {
        Init("NotUseNow");
    }
    HandleStatus::HandleStatus(const string &name)
    {
        Init(name);
    }
    HandleStatus::~HandleStatus()
    {
    }
    void HandleStatus::CallStart(ssize_t bytes)
    {
        if (mState != HANDLE_NONE && mState != HANDLE_DONE) {
            WRITE_LOG(LOG_FATAL, "this handle[%s] already do something now", mName.c_str());
            return;
        }
        mState = HANDLE_DOING;
        gettimeofday(&mInTime, NULL);
        if (bytes > 0)
        {
            mDealedBytes += bytes;
        }
    }
    void HandleStatus::CallEnd(void)
    {
        if (mState != HANDLE_DOING) {
            WRITE_LOG(LOG_FATAL, "this handle[%s] do nothing now", mName.c_str());
            return;
        }
        mState = HANDLE_DONE;
        gettimeofday(&mOutTime, NULL);
        int64_t duration = time_sub(mOutTime, mInTime);
        if (duration > 0) {
            mTotalDuration += duration;
        }
    }
    void HandleStatus::Display(const string &info) const
    {
        struct timeval now;
        int64_t hung = 0;
        if (time_ns(mOutTime) < time_ns(mInTime)) {
            gettimeofday(&now, NULL);
            hung = time_sub(now, mInTime);
        }
        WRITE_LOG(LOG_INFO, "handle[%s] %s [%s:%lldus] last[in:%llu~out:%llu]us duration[%llu]us bytes[%llu]",
                  mName.c_str(), info.c_str(), StateToString().c_str(), hung, time_ns(mInTime),
                  time_ns(mOutTime), mTotalDuration, mDealedBytes);
    }
    string HandleStatus::StateToString(void) const
    {
        const string stateString[] = { "none", "doing", "done" };
        return stateString[mState];
    }

    LoopStatus::LoopStatus(const string &name)
    {
        mName = name;
        mInitTime.tv_sec = 0;
        mInitTime.tv_usec = 0;
        mCloseTime.tv_sec = 0;
        mCloseTime.tv_usec = 0;
    }
    LoopStatus::~LoopStatus()
    {
        Display("life over:");
        mHandles.clear();
    }
    void LoopStatus::Init(void)
    {
        gettimeofday(&mInitTime, NULL);
    }
    void LoopStatus::Close(void)
    {
        gettimeofday(&mCloseTime, NULL);
    }
    void LoopStatus::AddHandle(const uintptr_t handle, const string &name)
    {
        if (!handle || name.empty())
        {
            WRITE_LOG(LOG_FATAL, "invalid param");
            return;
        }
        if (mHandles.count(handle))
        {
            WRITE_LOG(LOG_FATAL, "add %s failed, alread exist", name.c_str());
            return;
        }
        HandleStatus h(name);
        mHandles[handle] = h;
    }
    void LoopStatus::DelHandle(const uintptr_t handle)
    {
        if (!mHandles.count(handle))
        {
            WRITE_LOG(LOG_FATAL, "del handle failed, not exist");
            return;
        }
        mHandles[handle].Display("will delete:");
        mHandles.erase(handle);
    }
    void LoopStatus::HandleStart(const uintptr_t handle, ssize_t bytes)
    {
        if (!mHandles.count(handle))
        {
            WRITE_LOG(LOG_FATAL, "start handle failed, not exist");
            return;
        }
        mHandles[handle].CallStart(bytes);
    }
    void LoopStatus::HandleEnd(const uintptr_t handle)
    {
        if (!mHandles.count(handle))
        {
            WRITE_LOG(LOG_FATAL, "end handle failed, not exist");
            return;
        }
        mHandles[handle].CallEnd();
    }
    void LoopStatus::Display(const string &info) const
    {
        WRITE_LOG(LOG_INFO, "loop[%s] %s time[init:%llu~close:%llu~duration:%lld]us has [%d] handles:",
                  mName.c_str(), info.c_str(), time_ns(mInitTime), time_ns(mCloseTime),
                  time_sub(mCloseTime, mInitTime), mHandles.size());
        for (auto it : mHandles)
        {
            it.second.Display(info);
        }
    }

} /* namespace Hdc  */
