/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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
#include "cmd_log_wrapper.h"
#include <securec.h>
#include <chrono>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>

namespace {
constexpr size_t QUEUE_MAX = 1500;
std::mutex g_mutex;
std::queue<std::string> g_queue;
std::chrono::system_clock::time_point g_lastFlush = std::chrono::system_clock::now();
}

extern "C" {
void CmdLogPush(const char *logStr)
{
    if (logStr == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_queue.size() >= QUEUE_MAX) {
        return;
    }
    g_queue.push(std::string(logStr));
}

int CmdLogPop(char *out, int maxLen)
{
    if (out == nullptr || maxLen <= 0) {
        return -1;
    }
    std::string frontStr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_queue.empty()) {
            out[0] = '\0';
            return -1;
        }
        frontStr = std::move(g_queue.front());
        g_queue.pop();
        g_lastFlush = std::chrono::system_clock::now();
    }
    int copyLen = static_cast<int>(frontStr.size());
    if (copyLen > maxLen - 1) {
        copyLen = maxLen - 1;
    }
    if (strncpy_s(out, maxLen, frontStr.c_str(), copyLen) != EOK) {
        out[0] = '\0';
        return -1;
    }
    out[copyLen] = '\0';
    return copyLen;
}

uint32_t CmdLogSize()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return static_cast<uint32_t>(g_queue.size());
}
}
