/*
 * Copyright (C) 2021-2022 Huawei Device Co., Ltd.
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
 *
 */

#ifndef REGISTER_HDC_JDWP_H
#define REGISTER_HDC_JDWP_H

#include "define_register.h"

namespace Hdc {
class HdcJdwpSimulator;

class HdcJdwpSimulator {
public:
    explicit HdcJdwpSimulator(std::string processName, std::string pkgName, bool isDebug, Callback cb);
    ~HdcJdwpSimulator();
    bool Connect();
    void Disconnect();

protected:
    struct ContextJdwpSimulator {
        int cfd;
        HdcJdwpSimulator *thisClass;
    };
    using HCtxJdwpSimulator = struct ContextJdwpSimulator *;

private:
    struct JsMsgHeader {
        uint32_t msgLen;
        uint32_t pid;
        uint8_t isDebug; // 1:debug 0:release
    };
    void *MallocContext();
    static bool ConnectJpid(HdcJdwpSimulator *param);
    static bool SendToJpid(int fd, const uint8_t *buf, const int bufLen);
    HCtxJdwpSimulator ctxPoint_;
    std::string processName_;
    std::string pkgName_;
    bool isDebug_;
    Callback cb_;
    int cfd_;
    std::atomic<bool> disconnectFlag_;
    std::atomic<bool> startOnce_;
    std::thread readThread_;
    static void ReadWork(HdcJdwpSimulator *param);
    void Read();
    void ReadStart();
    void Reconnect();
};
} // namespace Hdc
#endif  // REGISTER_HDC_JDWP_H
