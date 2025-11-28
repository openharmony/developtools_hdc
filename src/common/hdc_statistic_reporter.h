/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef HDC_STATISTIC_REPORTER_H
#define HDC_STATISTIC_REPORTER_H

#include <mutex>
#include <string>
#include <vector>

#include <uv.h>

namespace Hdc {
const int STATISTIC_REPORT_COUNT = 34;
enum class STATISTIC_ITEM {
    HDC_VERSION,
    CONN_TYPE,
    HOST_OS,
    FILE_TRANSFER_SIZE,
    FILE_TRANSFER_COST,
    DISCONNECT_COUNT,
    FREEZE_COUNT,
    FREE_SESSION_MAX_COST,
    TCONN_COUNT,
    TCONN_FAIL_COUNT,
    INTERACT_SHELL_COUNT,
    INTERACT_SHELL_FAIL_COUNT,
    SHELL_COUNT,
    SHELL_FAIL_COUNT,
    INSTALL_COUNT,
    INSTALL_FAIL_COUNT,
    UNINSTALL_COUNT,
    UNINSTALL_FAIL_COUNT,
    FILE_SEND_COUNT,
    FILE_SEND_FAIL_COUNT,
    FILE_RECV_COUNT,
    FILE_RECV_FAIL_COUNT,
    FPORT_COUNT,
    FPORT_FAIL_COUNT,
    RPORT_COUNT,
    RPORT_FAIL_COUNT,
    FPORT_RM_COUNT,
    FPORT_RM_FAIL_COUNT,
    HILOG_COUNT,
    HILOG_FAIL_COUNT,
    JPID_COUNT,
    JPID_FAIL_COUNT,
    TRACK_JPID_COUNT,
    TRACK_JPID_FAIL_COUNT,
};

class HdcStatisticReporter {
public:
    static HdcStatisticReporter& GetInstance();
    void Schedule(uv_loop_t* loop);
    void SetConnectInfo(const std::vector<std::string>& features);
    void IncrFileTransferInfo(uint64_t fileSize, int fileCost);
    void UpdateFreeSessionMaxCost(int freeCost);
    void IncrCommandInfo(STATISTIC_ITEM command);
private:
    HdcStatisticReporter() = default;
    ~HdcStatisticReporter();

    static void HandleReport(uv_timer_t* timer);
    void Report();
    void Clear();
    bool CanReport();

    [[maybe_unused]] std::string hdcVersion_ = "";
    [[maybe_unused]] std::string connType_ = "";
    [[maybe_unused]] std::string hostOs_ = "";
    [[maybe_unused]] uint64_t fileTransferSize_ = 0;
    [[maybe_unused]] int fileTransferCost_ = 0;
    [[maybe_unused]] int freeSessionMaxCost_ = 0;
    [[maybe_unused]] int eventCnt_[STATISTIC_REPORT_COUNT] = { 0 };
    [[maybe_unused]] std::mutex mutex_;

    HdcStatisticReporter(const HdcStatisticReporter&) = delete;
    HdcStatisticReporter& operator=(const HdcStatisticReporter&) = delete;
    HdcStatisticReporter(HdcStatisticReporter&&) = delete;
    HdcStatisticReporter& operator=(HdcStatisticReporter&&) = delete;
};
}   // namespace Hdc
#endif  // HDC_STATISTIC_REPORTER_H