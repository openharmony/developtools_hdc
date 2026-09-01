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
#include "statistic_wrapper.h"
#include <securec.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <sstream>

namespace {
std::mutex g_mutex;
uint64_t g_items[STAT_ITEM_MAX] = {0};
std::chrono::system_clock::time_point g_lastReport = std::chrono::system_clock::now();

const char *StatName(uint32_t id)
{
    static const char *names[STAT_ITEM_MAX] = {
        "conn_usb", "conn_tcp", "conn_uart",
        "file_send_size", "file_send_cost", "file_recv_size", "file_recv_cost",
        "shell_count", "shell_fail",
        "install_count", "install_fail",
        "uninstall_count", "uninstall_fail",
        "fport_count", "fport_fail",
        "hilog_count", "hilog_fail",
        "jpid_count", "jpid_fail",
        "freeze_count", "disconnect_count",
    };
    if (id >= STAT_ITEM_MAX) {
        return "unknown";
    }
    return names[id];
}
}

extern "C" {
void StatInc(uint32_t itemId)
{
    if (itemId >= STAT_ITEM_MAX) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_items[itemId]++;
}

void StatAdd(uint32_t itemId, uint64_t val)
{
    if (itemId >= STAT_ITEM_MAX) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_items[itemId] += val;
}

uint64_t StatGet(uint32_t itemId)
{
    if (itemId >= STAT_ITEM_MAX) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_items[itemId];
}

void StatReset()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (memset_s(g_items, sizeof(g_items), 0, sizeof(g_items)) != EOK) { return; }
    g_lastReport = std::chrono::system_clock::now();
}

void StatReport()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ostringstream ss;
    ss << "=== HDC Statistics Report ===" << std::endl;
    for (uint32_t i = 0; i < STAT_ITEM_MAX; i++) {
        if (g_items[i] > 0) {
            ss << "  " << StatName(i) << ": " << g_items[i] << std::endl;
        }
    }
    ss << "=== End of Report ===" << std::endl;
    g_lastReport = std::chrono::system_clock::now();
}
}
