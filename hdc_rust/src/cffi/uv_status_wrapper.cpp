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
#include "uv_status_wrapper.h"
#include <securec.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <sstream>
#include <thread>

namespace {
constexpr uint64_t HUNG_THRESHOLD_MS = 30000;
struct LoopStat {
    char name[64] = {0};
    uint64_t busyCount = 0;
    uint64_t totalDuration = 0;
    uint64_t currentStart = 0;
    bool inProgress = false;
    bool isHung = false;
};

struct CallStat {
    char name[64] = {0};
    uint64_t callCount = 0;
    uint64_t totalCostMs = 0;
    uint64_t maxCostMs = 0;
};

std::mutex g_mutex;
LoopStat g_loopStats[LOOP_STATUS_MAX];
CallStat g_callStats[CALL_STAT_MAX];
uint32_t g_loopCount = 0;
uint32_t g_callCount = 0;
std::atomic<bool> g_monitorRunning{false};
std::thread g_monitorThread;

uint64_t NowMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

int FindLoopSlot(const char *name)
{
    for (uint32_t i = 0; i < g_loopCount; i++) {
        if (strncmp(g_loopStats[i].name, name, sizeof(LoopStat::name)) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int FindCallSlot(const char *name)
{
    for (uint32_t i = 0; i < g_callCount; i++) {
        if (strncmp(g_callStats[i].name, name, sizeof(CallStat::name)) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int AllocLoopSlot(const char *name)
{
    if (g_loopCount >= LOOP_STATUS_MAX) {
        return -1;
    }
    int slot = static_cast<int>(g_loopCount);
    if (strncpy_s(g_loopStats[slot].name, sizeof(LoopStat::name),
        name, sizeof(LoopStat::name) - 1) != EOK) {
        return -1;
    }
    g_loopCount++;
    return slot;
}

int AllocCallSlot(const char *name)
{
    if (g_callCount >= CALL_STAT_MAX) {
        return -1;
    }
    int slot = static_cast<int>(g_callCount);
    if (strncpy_s(g_callStats[slot].name, sizeof(CallStat::name),
        name, sizeof(CallStat::name) - 1) != EOK) {
        return -1;
    }
    g_callCount++;
    return slot;
}

void CheckHungStatus()
{
    uint64_t now = NowMs();
    for (uint32_t i = 0; i < g_loopCount; i++) {
        if (!g_loopStats[i].inProgress || g_loopStats[i].currentStart == 0) {
            continue;
        }
        uint64_t elapsed = now - g_loopStats[i].currentStart;
        if (elapsed > HUNG_THRESHOLD_MS) {
            g_loopStats[i].isHung = true;
        }
    }
}

void MonitorLoop(uint32_t intervalMs)
{
    while (g_monitorRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        std::lock_guard<std::mutex> lock(g_mutex);
        CheckHungStatus();
    }
}
}

extern "C" {
void LoopStatusInit(const char *name)
{
    if (name == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (FindLoopSlot(name) >= 0) {
        return;
    }
    AllocLoopSlot(name);
}

void LoopStatusHandleStart(const char *name)
{
    if (name == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    int slot = FindLoopSlot(name);
    if (slot < 0) {
        slot = AllocLoopSlot(name);
    }
    if (slot >= 0) {
        g_loopStats[slot].inProgress = true;
        g_loopStats[slot].currentStart = NowMs();
    }
}

void LoopStatusHandleEnd(const char *name)
{
    if (name == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    int slot = FindLoopSlot(name);
    if (slot < 0) {
        return;
    }
    if (g_loopStats[slot].inProgress && g_loopStats[slot].currentStart > 0) {
        uint64_t cost = NowMs() - g_loopStats[slot].currentStart;
        g_loopStats[slot].totalDuration += cost;
        g_loopStats[slot].busyCount++;
        g_loopStats[slot].inProgress = false;
        g_loopStats[slot].isHung = false;
        g_loopStats[slot].currentStart = 0;
    }
}

bool LoopStatusIsHung(const char *name, uint64_t thresholdMs)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    int slot = FindLoopSlot(name);
    if (slot < 0) {
        return false;
    }
    if (!g_loopStats[slot].inProgress) {
        return false;
    }
    uint64_t elapsed = NowMs() - g_loopStats[slot].currentStart;
    return elapsed > thresholdMs;
}

void LoopStatusDisplay(const char *name)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ostringstream ss;
    for (uint32_t i = 0; i < g_loopCount; i++) {
        if (name != nullptr && strncmp(g_loopStats[i].name, name, sizeof(LoopStat::name)) != 0) {
            continue;
        }
        ss << "Loop[" << g_loopStats[i].name << "] busy=" <<
            g_loopStats[i].busyCount << " totalMs=" <<
            g_loopStats[i].totalDuration << " hung=" <<
            (g_loopStats[i].isHung ? "yes" : "no") << std::endl;
    }
}

uint32_t LoopStatusGetAll(LoopStatusInfo *out, uint32_t maxCount)
{
    if (out == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    uint32_t copyCount = (maxCount < g_loopCount) ? maxCount : g_loopCount;
    for (uint32_t i = 0; i < copyCount; i++) {
        if (strncpy_s(out[i].name, sizeof(LoopStatusInfo::name),
            g_loopStats[i].name,
            sizeof(LoopStatusInfo::name) - 1) != EOK) {
            return copyCount;
        }
        out[i].name[sizeof(LoopStatusInfo::name) - 1] = '\0';
        out[i].busyCount = g_loopStats[i].busyCount;
        out[i].totalDuration = g_loopStats[i].totalDuration;
        out[i].isHung = g_loopStats[i].isHung;
    }
    return copyCount;
}

void CallStatRecord(const char *name, uint64_t costMs)
{
    if (name == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    int slot = FindCallSlot(name);
    if (slot < 0) {
        slot = AllocCallSlot(name);
    }
    if (slot >= 0) {
        g_callStats[slot].callCount++;
        g_callStats[slot].totalCostMs += costMs;
        if (costMs > g_callStats[slot].maxCostMs) {
            g_callStats[slot].maxCostMs = costMs;
        }
    }
}

uint32_t CallStatGetAll(CallStatEntry *out, uint32_t maxCount)
{
    if (out == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    uint32_t copyCount = (maxCount < g_callCount) ? maxCount : g_callCount;
    for (uint32_t i = 0; i < copyCount; i++) {
        if (strncpy_s(out[i].name, sizeof(CallStatEntry::name),
            g_callStats[i].name,
            sizeof(CallStatEntry::name) - 1) != EOK) {
            return copyCount;
        }
        out[i].name[sizeof(CallStatEntry::name) - 1] = '\0';
        out[i].callCount = g_callStats[i].callCount;
        out[i].totalCostMs = g_callStats[i].totalCostMs;
        out[i].maxCostMs = g_callStats[i].maxCostMs;
    }
    return copyCount;
}

void CallStatReset()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (memset_s(g_callStats, sizeof(g_callStats), 0, sizeof(g_callStats)) != EOK) { return; }
    g_callCount = 0;
}

void LoopMonitorStart(uint32_t intervalMs)
{
    if (g_monitorRunning.exchange(true)) {
        return;
    }
    g_monitorThread = std::thread(MonitorLoop, intervalMs);
    g_monitorThread.detach();
}

void LoopMonitorStop()
{
    g_monitorRunning = false;
}
}
