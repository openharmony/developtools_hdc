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

#include "hdc_statistic_reporter.h"

#ifdef HDC_STATISTIC_REPORT_ENABLE
#include <limits>
#include <securec.h>

#include "hisysevent_c.h"

#include "base.h"
#include "log.h"
#endif

namespace Hdc {
HdcStatisticReporter& HdcStatisticReporter::GetInstance()
{
    static HdcStatisticReporter reporter;
    return reporter;
}

void HdcStatisticReporter::Schedule(uv_loop_t* loop)
{
#ifdef HDC_STATISTIC_REPORT_ENABLE
    const int event_report_interval = 6 * 60 * 60 * 1000;   // 6 hours
    uv_timer_t* timer = new(std::nothrow) uv_timer_t();
    if (timer == nullptr) {
        WRITE_LOG(LOG_FATAL, "HdcStatisticReporter timer init failed");
        return;
    }
    uv_timer_init(loop, timer);
    uv_timer_start(timer, HandleReport, event_report_interval, event_report_interval);
#endif
}

void HdcStatisticReporter::SetConnectInfo(const std::vector<std::string>& features)
{
#ifdef HDC_STATISTIC_REPORT_ENABLE
    std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t hdcVersionIndex = 0;
    const std::size_t connTypeIndex = 1;
    const std::size_t hostOsIndex = 2;
    const std::size_t connInfoCount = 3;
    if (features.size() >= connInfoCount) {
        hdcVersion_ = features[hdcVersionIndex];
        connType_ = features[connTypeIndex];
        hostOs_ = features[hostOsIndex];
    }
#endif
}

void HdcStatisticReporter::IncrFileTransferInfo(uint64_t fileSize, int fileCost)
{
#ifdef HDC_STATISTIC_REPORT_ENABLE
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t maxVal = std::numeric_limits<uint64_t>::max();
    // avoid overflow
    if (maxVal - fileTransferSize_ < fileSize) {
        fileTransferSize_ = maxVal;
    } else {
        fileTransferSize_ += fileSize;
    }
    fileTransferCost_ += fileCost;
#endif
}

void HdcStatisticReporter::UpdateFreeSessionMaxCost(int freeCost)
{
#ifdef HDC_STATISTIC_REPORT_ENABLE
    std::lock_guard<std::mutex> lock(mutex_);
    if (freeCost > freeSessionMaxCost_) {
        freeSessionMaxCost_ = freeCost;
    }
#endif
}

void HdcStatisticReporter::IncrCommandInfo(STATISTIC_ITEM command)
{
#ifdef HDC_STATISTIC_REPORT_ENABLE
    std::lock_guard<std::mutex> lock(mutex_);
    eventCnt_[static_cast<int>(command)]++;
#endif
}

HdcStatisticReporter::~HdcStatisticReporter()
{
#ifdef HDC_STATISTIC_REPORT_ENABLE
    Report();
#endif
}

void HdcStatisticReporter::Clear()
{
#ifdef HDC_STATISTIC_REPORT_ENABLE
    fileTransferSize_ = 0;
    fileTransferCost_ = 0;
    freeSessionMaxCost_ = 0;
    (void)memset_s(eventCnt_, sizeof(eventCnt_), 0, sizeof(eventCnt_));
#endif
}

void HdcStatisticReporter::HandleReport(uv_timer_t* timer)
{
#ifdef HDC_STATISTIC_REPORT_ENABLE
    GetInstance().Report();
#endif
}

void HdcStatisticReporter::Report()
{
#ifdef HDC_STATISTIC_REPORT_ENABLE
    std::lock_guard<std::mutex> lock(mutex_);
    if (!CanReport()) {
        return;
    }
    HiSysEventParam params[] = {
        {
            .name = "HDC_VERSION",
            .t = HISYSEVENT_STRING,
            .v = {.s = const_cast<char*>(hdcVersion_.c_str())},
            .arraySize = 0
        },
        {
            .name = "CONN_TYPE",
            .t = HISYSEVENT_STRING,
            .v = {.s = const_cast<char*>(connType_.c_str())},
            .arraySize = 0
        },
        {
            .name = "HOST_OS",
            .t = HISYSEVENT_STRING,
            .v = {.s = const_cast<char*>(hostOs_.c_str())},
            .arraySize = 0
        },
        {
            .name = "FILE_TRANSFER_SIZE",
            .t = HISYSEVENT_UINT64,
            .v = {.ui64 = fileTransferSize_},
            .arraySize = 0
        },
        {
            .name = "FILE_TRANSFER_COST",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = fileTransferCost_},
            .arraySize = 0
        },
        {
            .name = "DISCONNECT_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::DISCONNECT_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "FREEZE_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::FREEZE_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "FREE_SESSION_MAX_COST",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = freeSessionMaxCost_},
            .arraySize = 0
        },
        {
            .name = "TCONN_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::TCONN_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "TCONN_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::TCONN_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "INTERACT_SHELL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::INTERACT_SHELL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "INTERACT_SHELL_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::INTERACT_SHELL_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "SHELL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::SHELL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "SHELL_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::SHELL_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "INSTALL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::INSTALL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "INSTALL_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::INSTALL_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "UNINSTALL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::UNINSTALL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "UNINSTALL_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::UNINSTALL_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "FILE_SEND_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::FILE_SEND_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "FILE_SEND_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::FILE_SEND_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "FILE_RECV_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::FILE_RECV_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "FILE_RECV_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::FILE_RECV_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "FPORT_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::FPORT_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "FPORT_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::FPORT_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "RPORT_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::RPORT_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "RPORT_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::RPORT_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "FPORT_RM_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::FPORT_RM_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "FPORT_RM_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::FPORT_RM_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "HILOG_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::HILOG_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "HILOG_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::HILOG_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "JPID_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::JPID_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "JPID_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::JPID_FAIL_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "TRACK_JPID_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::TRACK_JPID_COUNT)]},
            .arraySize = 0
        },
        {
            .name = "TRACK_JPID_FAIL_COUNT",
            .t = HISYSEVENT_INT32,
            .v = {.i32 = eventCnt_[static_cast<int>(STATISTIC_ITEM::TRACK_JPID_FAIL_COUNT)]},
            .arraySize = 0
        }
    };

    int ret = OH_HiSysEvent_Write("HDC", "DEVICE_HDCD_STATS",
        HISYSEVENT_STATISTIC, params, sizeof(params) / sizeof(params[0]));
    if (ret != 0) {
        WRITE_LOG(LOG_FATAL, "Report HdcStatisticReporter failed, ret: %d", ret);
        return;
    }
    // clear after report
    Clear();
#endif
}

bool HdcStatisticReporter::CanReport()
{
#ifdef HDC_STATISTIC_REPORT_ENABLE
    bool empty = true;
    for (int i = 0; i < STATISTIC_REPORT_COUNT; ++i) {
        if (eventCnt_[i] > 0) {
            empty = false;
            break;
        }
    }
    return hdcVersion_ != "" &&
           connType_ != "" &&
           hostOs_ != "" &&
           (fileTransferSize_ > 0 ||
           fileTransferCost_ > 0 ||
           freeSessionMaxCost_ > 0 ||
           !empty);
#else
    return false;
#endif
}
}