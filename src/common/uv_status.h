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

#ifndef __H_UV_STATUS_H__
#define __H_UV_STATUS_H__

#include <cinttypes>
#include <string>
#include <sys/time.h>
#include <map>
#include <uv.h>

using std::string;

namespace Hdc {

#define MS_PER_SEC 1000
#define US_PER_SEC (1000 * 1000)

class LoopStatus {
public:
    LoopStatus(uv_loop_t *loop, const string &loopName);
    ~LoopStatus();
private:
    bool Busy(void) const;
public:
    void HandleStart(const uv_loop_t *loop, const string &handle);
    void HandleEnd(const uv_loop_t *loop);
    void Display(const string &info) const;
    void HungCheck(int64_t timeout) const;
    void UnUsedForUpdater(void) const {}
public:
    static void MonitorTimerProc(uv_timer_t *req);
private:
    uv_loop_t *mLoop;
    const string mLoopName;
    string mHandleName;
    bool mBusyNow;
    struct timeval mCallBackTime;
    uv_timer_t mMonitorTimer;
};

class CallStatGuard {
public:
    CallStatGuard(LoopStatus &loopStatus, const uv_loop_t *loop, const string &handle) : mCommitted(false),
                                                                                         mLoop(loop),
                                                                                         mLoopStatus(loopStatus)
    {
        mLoopStatus.HandleStart(loop, handle);
    }
    ~CallStatGuard()
    {
        if (mCommitted) {
            return;
        }
        mLoopStatus.HandleEnd(mLoop);
    }
    void Commit(void)
    {
        if (mCommitted) {
            return;
        }
        mLoopStatus.HandleEnd(mLoop);
        mCommitted = true;
    }
private:
    bool mCommitted;
    const uv_loop_t *mLoop;
    LoopStatus &mLoopStatus;
};

void DispAllLoopStatus(const string &info);
#ifdef UPDATER_MODE
#define CALLSTAT_GUARD(ls, loop, funcname) (ls).UnUsedForUpdater()
#else
#define CALLSTAT_GUARD(ls, loop, funcname) CallStatGuard csg(ls, loop, funcname)
#endif
void StartLoopMonitor(void);
int GetLoopHungTimeout(void);
int GetLoopMonitorPeriod(void);

} /* namespace Hdc  */

#endif /* __H_UV_STATUS_H__ */
