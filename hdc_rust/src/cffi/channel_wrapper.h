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
#ifndef HDC_CHANNEL_WRAPPER_H
#define HDC_CHANNEL_WRAPPER_H
#include <cstdint>

constexpr uint16_t CH_ADMIN_ADD = 0;
constexpr uint16_t CH_ADMIN_REMOVE = 1;
constexpr uint16_t CH_ADMIN_QUERY = 2;
constexpr uint16_t CH_ADMIN_GET_ONLY = 9;

extern "C" {
int ChannelAdmin(uint16_t operation, uint32_t sessionId, int channelId);
int ChannelSendToSession(uint32_t sessionId, const uint8_t *data, int len);
int ChannelSendEcho(uint32_t sessionId, const uint8_t *data, int len);
int ChannelCloseIdle(uint32_t sessionId, uint32_t idleMs);
void ChannelEchoToAll(uint32_t sessionId, const uint8_t *data, int len);
uint32_t ChannelGetCount(uint32_t sessionId);
int ChannelSetHandshake(uint32_t sessionId, const uint8_t *data, int len);
}

#endif
