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
#include "channel_wrapper.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <queue>
#include <vector>

namespace {
struct ChannelEntry {
    int channelId = -1;
    bool active = false;
    uint64_t lastActiveMs = 0;
    std::queue<std::vector<uint8_t>> sendQueue;
    bool echoEnabled = false;
};

struct SessionEntry {
    std::vector<ChannelEntry> channels;
    std::vector<uint8_t> handshake;
};

std::mutex g_mutex;
std::map<uint32_t, SessionEntry> g_sessions;

uint64_t NowMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

ChannelEntry *FindChannel(SessionEntry &session, int channelId)
{
    for (auto &ch : session.channels) {
        if (ch.channelId == channelId) {
            return &ch;
        }
    }
    return nullptr;
}

void InternalSendToChannel(ChannelEntry &ch, const uint8_t *data, int len)
{
    if (data == nullptr || len <= 0) {
        return;
    }
    ch.sendQueue.push(std::vector<uint8_t>(data, data + len));
    ch.lastActiveMs = NowMs();
}
}

extern "C" {
int ChannelAdmin(uint16_t operation, uint32_t sessionId, int channelId)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sessions.find(sessionId);
    switch (operation) {
        case CH_ADMIN_ADD: {
            if (it == g_sessions.end()) {
                it = g_sessions.insert({sessionId, SessionEntry{}}).first;
            }
            ChannelEntry ch;
            ch.channelId = channelId;
            ch.active = true;
            ch.lastActiveMs = NowMs();
            it->second.channels.push_back(ch);
            return 0;
        }
        case CH_ADMIN_REMOVE: {
            if (it == g_sessions.end()) {
                return -1;
            }
            for (auto iter = it->second.channels.begin(); iter != it->second.channels.end(); ++iter) {
                if (iter->channelId == channelId) {
                    iter->active = false;
                    it->second.channels.erase(iter);
                    return 0;
                }
            }
            return -1;
        }
        case CH_ADMIN_QUERY: {
            if (it == g_sessions.end()) {
                return -1;
            }
            return FindChannel(it->second, channelId) != nullptr ? 1 : 0;
        }
        case CH_ADMIN_GET_ONLY: {
            if (it == g_sessions.end() || it->second.channels.empty()) {
                return -1;
            }
            return it->second.channels.front().channelId;
        }
        default:
            return -1;
    }
}

int ChannelSendToSession(uint32_t sessionId, const uint8_t *data, int len)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sessions.find(sessionId);
    if (it == g_sessions.end() || it->second.channels.empty()) {
        return -1;
    }
    ChannelEntry *ch = &it->second.channels.front();
    if (!ch->active) {
        return -1;
    }
    InternalSendToChannel(*ch, data, len);
    return len;
}

int ChannelSendEcho(uint32_t sessionId, const uint8_t *data, int len)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sessions.find(sessionId);
    if (it == g_sessions.end()) {
        return -1;
    }
    for (auto &ch : it->second.channels) {
        if (ch.echoEnabled && ch.active) {
            InternalSendToChannel(ch, data, len);
        }
    }
    return len;
}

int ChannelCloseIdle(uint32_t sessionId, uint32_t idleMs)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sessions.find(sessionId);
    if (it == g_sessions.end()) {
        return -1;
    }
    uint64_t now = NowMs();
    int closedCount = 0;
    for (auto &ch : it->second.channels) {
        if (ch.active && (now - ch.lastActiveMs) > idleMs) {
            ch.active = false;
            while (!ch.sendQueue.empty()) {
                ch.sendQueue.pop();
            }
            closedCount++;
        }
    }
    return closedCount;
}

void ChannelEchoToAll(uint32_t sessionId, const uint8_t *data, int len)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sessions.find(sessionId);
    if (it == g_sessions.end()) {
        return;
    }
    for (auto &ch : it->second.channels) {
        if (ch.active) {
            InternalSendToChannel(ch, data, len);
        }
    }
}

uint32_t ChannelGetCount(uint32_t sessionId)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sessions.find(sessionId);
    if (it == g_sessions.end()) {
        return 0;
    }
    uint32_t count = 0;
    for (const auto &ch : it->second.channels) {
        if (ch.active) {
            count++;
        }
    }
    return count;
}

int ChannelSetHandshake(uint32_t sessionId, const uint8_t *data, int len)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_sessions.find(sessionId);
    if (it == g_sessions.end()) {
        it = g_sessions.insert({sessionId, SessionEntry{}}).first;
    }
    if (data == nullptr || len <= 0) {
        return -1;
    }
    it->second.handshake.assign(data, data + len);
    return len;
}
}
