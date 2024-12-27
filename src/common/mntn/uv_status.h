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

#include "common.h"

namespace Hdc {

class HandleStatus {
public:
    HandleStatus();
    HandleStatus(const string &name);
    ~HandleStatus();
private:
    void Init(const string &name);
public:
    void CallStart(ssize_t bytes);
    void CallEnd(void);
    void Display(const string &info) const;
private:
    enum HandleState {
        HANDLE_NONE = 0,
        HANDLE_DOING = 1,
        HANDLE_DONE = 2,
    };
    string StateToString(void) const;
private:
    string mName;
    HandleState mState;
    struct timeval mInTime;
    struct timeval mOutTime;
    uint64_t mTotalDuration;
    uint64_t mDealedBytes;
};

class LoopStatus {
public:
    LoopStatus(const string &name);
    ~LoopStatus();
private:
    void Init(void);
public:
    void Close(void);
    void AddHandle(const uintptr_t handle, const string &name);
    void DelHandle(const uintptr_t handle);
    void HandleStart(const uintptr_t handle, ssize_t bytes);
    void HandleEnd(const uintptr_t handle);
    void Display(const string &info) const;
private:
    string mName;
    struct timeval mInitTime;
    struct timeval mCloseTime;
    std::map<uintptr_t, HandleStatus> mHandles;
};

} /* namespace Hdc  */

#endif /* __H_UV_STATUS_H__ */
