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
#include "heartbeat_wrapper.h"
#include <chrono>

namespace Hdc {
static std::chrono::steady_clock::time_point g_lastTime = std::chrono::steady_clock::now();
static constexpr uint64_t HEARTBEAT_TIMEOUT_SECONDS = 3600;
}

extern "C" {
void HbAddCount(Hdc::HeartbeatState *state)
{
    if (state == nullptr) {
        return;
    }
    state->heartbeatCount++;
}

bool HbHandleMessage(Hdc::HeartbeatState *state)
{
    if (state == nullptr) {
        return false;
    }
    state->messageCount++;
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - Hdc::g_lastTime);
    Hdc::g_lastTime = now;
    if (duration.count() > static_cast<int64_t>(Hdc::HEARTBEAT_TIMEOUT_SECONDS)) {
        return false;
    }
    return true;
}

void HbSetSupport(Hdc::HeartbeatState *state, bool enable)
{
    if (state == nullptr) {
        return;
    }
    state->supportHeartbeat = enable;
}

bool HbGetSupport(const Hdc::HeartbeatState *state)
{
    if (state == nullptr) {
        return false;
    }
    return state->supportHeartbeat;
}

uint64_t HbGetCount(const Hdc::HeartbeatState *state)
{
    if (state == nullptr) {
        return 0;
    }
    return state->heartbeatCount;
}
}
