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
#ifndef HDC_HEARTBEAT_WRAPPER_H
#define HDC_HEARTBEAT_WRAPPER_H
#include <cstdint>

namespace Hdc {
struct HeartbeatState {
    uint64_t heartbeatCount;
    uint64_t messageCount;
    bool supportHeartbeat;
};
}

extern "C" {
void HbAddCount(Hdc::HeartbeatState *state);
bool HbHandleMessage(Hdc::HeartbeatState *state);
void HbSetSupport(Hdc::HeartbeatState *state, bool enable);
bool HbGetSupport(const Hdc::HeartbeatState *state);
uint64_t HbGetCount(const Hdc::HeartbeatState *state);
}

#endif
