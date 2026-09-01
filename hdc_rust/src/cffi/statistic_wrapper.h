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
#ifndef HDC_STATISTIC_WRAPPER_H
#define HDC_STATISTIC_WRAPPER_H
#include <cstdint>

constexpr uint32_t STAT_ITEM_MAX = 34;

enum StatItemId {
    STAT_CONN_USB = 0,
    STAT_CONN_TCP,
    STAT_CONN_UART,
    STAT_FILE_SEND_SIZE,
    STAT_FILE_SEND_COST,
    STAT_FILE_RECV_SIZE,
    STAT_FILE_RECV_COST,
    STAT_SHELL_COUNT,
    STAT_SHELL_FAIL,
    STAT_INSTALL_COUNT,
    STAT_INSTALL_FAIL,
    STAT_UNINSTALL_COUNT,
    STAT_UNINSTALL_FAIL,
    STAT_FPORT_COUNT,
    STAT_FPORT_FAIL,
    STAT_HILOG_COUNT,
    STAT_HILOG_FAIL,
    STAT_JPID_COUNT,
    STAT_JPID_FAIL,
    STAT_FREEZE_COUNT,
    STAT_DISCONNECT_COUNT,
};

extern "C" {
void StatInc(uint32_t itemId);
void StatAdd(uint32_t itemId, uint64_t val);
uint64_t StatGet(uint32_t itemId);
void StatReset();
void StatReport();
}

#endif
