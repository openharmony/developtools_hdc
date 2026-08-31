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
#ifndef HDC_UV_STATUS_WRAPPER_H
#define HDC_UV_STATUS_WRAPPER_H
#include <cstdint>

constexpr uint32_t LOOP_STATUS_MAX = 32;
constexpr uint32_t CALL_STAT_MAX = 64;

struct LoopStatusInfo {
    char name[64];
    uint64_t busyCount;
    uint64_t totalDuration;
    bool isHung;
};

struct CallStatEntry {
    char name[64];
    uint64_t callCount;
    uint64_t totalCostMs;
    uint64_t maxCostMs;
};

extern "C" {
void LoopStatusInit(const char *name);
void LoopStatusHandleStart(const char *name);
void LoopStatusHandleEnd(const char *name);
bool LoopStatusIsHung(const char *name, uint64_t thresholdMs);
void LoopStatusDisplay(const char *name);
uint32_t LoopStatusGetAll(LoopStatusInfo *out, uint32_t maxCount);

void CallStatRecord(const char *name, uint64_t costMs);
uint32_t CallStatGetAll(CallStatEntry *out, uint32_t maxCount);
void CallStatReset();

void LoopMonitorStart(uint32_t intervalMs);
void LoopMonitorStop();
}

#endif
