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
#include "hdc_statistic_reporter_ut.h"

using namespace testing::ext;

namespace Hdc {
void HdcStatisticReporterTest::SetUpTestCase() {}
void HdcStatisticReporterTest::TearDownTestCase() {}
void HdcStatisticReporterTest::SetUp() {}
void HdcStatisticReporterTest::TearDown()
{
    reporter_.Clear();
}

HWTEST_F(HdcStatisticReporterTest, Test_SetConnectInfo_1, TestSize.Level0)
{
    reporter_.SetConnectInfo({"3.2.0b", "USB", "win"});
    EXPECT_EQ(reporter_.hdcVersion_, "3.2.0b");
    EXPECT_EQ(reporter_.connType_, "USB");
    EXPECT_EQ(reporter_.hostOs_, "win");
}

HWTEST_F(HdcStatisticReporterTest, Test_SetConnectInfo_2, TestSize.Level0)
{
    reporter_.SetConnectInfo({"3.2.0b", "USB", "linux"});
    EXPECT_EQ(reporter_.hdcVersion_, "3.2.0b");
    EXPECT_EQ(reporter_.connType_, "USB");
    EXPECT_EQ(reporter_.hostOs_, "linux");
}

HWTEST_F(HdcStatisticReporterTest, Test_SetConnectInfo_3, TestSize.Level0)
{
    reporter_.SetConnectInfo({"3.2.0b", "USB", "mac"});
    EXPECT_EQ(reporter_.hdcVersion_, "3.2.0b");
    EXPECT_EQ(reporter_.connType_, "USB");
    EXPECT_EQ(reporter_.hostOs_, "mac");
}

HWTEST_F(HdcStatisticReporterTest, Test_SetConnectInfo_4, TestSize.Level0)
{
    reporter_.SetConnectInfo({"3.2.0b", "USB", "oh"});
    EXPECT_EQ(reporter_.hdcVersion_, "3.2.0b");
    EXPECT_EQ(reporter_.connType_, "USB");
    EXPECT_EQ(reporter_.hostOs_, "oh");
}

HWTEST_F(HdcStatisticReporterTest, Test_SetConnectInfo_5, TestSize.Level0)
{
    reporter_.SetConnectInfo({"3.2.0b", "TCP", "win"});
    EXPECT_EQ(reporter_.hdcVersion_, "3.2.0b");
    EXPECT_EQ(reporter_.connType_, "TCP");
    EXPECT_EQ(reporter_.hostOs_, "win");
}

HWTEST_F(HdcStatisticReporterTest, Test_SetConnectInfo_6, TestSize.Level0)
{
    reporter_.SetConnectInfo({"3.2.0b", "TCP", "linux"});
    EXPECT_EQ(reporter_.hdcVersion_, "3.2.0b");
    EXPECT_EQ(reporter_.connType_, "TCP");
    EXPECT_EQ(reporter_.hostOs_, "linux");
}

HWTEST_F(HdcStatisticReporterTest, Test_SetConnectInfo_7, TestSize.Level0)
{
    reporter_.SetConnectInfo({"3.2.0b", "TCP", "mac"});
    EXPECT_EQ(reporter_.hdcVersion_, "3.2.0b");
    EXPECT_EQ(reporter_.connType_, "TCP");
    EXPECT_EQ(reporter_.hostOs_, "mac");
}

HWTEST_F(HdcStatisticReporterTest, Test_SetConnectInfo_8, TestSize.Level0)
{
    reporter_.SetConnectInfo({"3.2.0b", "TCP", "oh"});
    EXPECT_EQ(reporter_.hdcVersion_, "3.2.0b");
    EXPECT_EQ(reporter_.connType_, "TCP");
    EXPECT_EQ(reporter_.hostOs_, "oh");
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrFileTransferInfo_1, TestSize.Level0)
{
    // once
    reporter_.IncrFileTransferInfo(1, 1);
    EXPECT_EQ(reporter_.fileTransferSize_, 1);
    EXPECT_EQ(reporter_.fileTransferCost_, 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrFileTransferInfo_2, TestSize.Level0)
{
    // more than once
    reporter_.IncrFileTransferInfo(1, 1);
    reporter_.IncrFileTransferInfo(1, 1);
    reporter_.IncrFileTransferInfo(1, 1);
    EXPECT_EQ(reporter_.fileTransferSize_, 3);
    EXPECT_EQ(reporter_.fileTransferCost_, 3);
}

HWTEST_F(HdcStatisticReporterTest, Test_UpdateFreeSessionMaxCost, TestSize.Level0)
{
    // more than once
    reporter_.UpdateFreeSessionMaxCost(1);
    EXPECT_EQ(reporter_.freeSessionMaxCost_, 1);

    reporter_.UpdateFreeSessionMaxCost(2);
    EXPECT_EQ(reporter_.freeSessionMaxCost_, 2);

    reporter_.UpdateFreeSessionMaxCost(1);
    EXPECT_EQ(reporter_.freeSessionMaxCost_, 2);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_DISCONNECT_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::DISCONNECT_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::DISCONNECT_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_FREEZE_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::FREEZE_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::FREEZE_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_TCONN_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::TCONN_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::TCONN_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_TCONN_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::TCONN_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::TCONN_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_INTERACT_SHELL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::INTERACT_SHELL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::INTERACT_SHELL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_INTERACT_SHELL_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::INTERACT_SHELL_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::INTERACT_SHELL_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_SHELL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::SHELL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::SHELL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_SHELL_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::SHELL_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::SHELL_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_INSTALL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::INSTALL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::INSTALL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_INSTALL_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::INSTALL_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::INSTALL_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_UNINSTALL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::UNINSTALL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::UNINSTALL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_UNINSTALL_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::UNINSTALL_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::UNINSTALL_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_FILE_SEND_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::FILE_SEND_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::FILE_SEND_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_FILE_SEND_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::FILE_SEND_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::FILE_SEND_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_FILE_RECV_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::FILE_RECV_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::FILE_RECV_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_FILE_RECV_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::FILE_RECV_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::FILE_RECV_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_FPORT_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::FPORT_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::FPORT_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_FPORT_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::FPORT_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::FPORT_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_RPORT_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::RPORT_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::RPORT_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_RPORT_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::RPORT_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::RPORT_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_FPORT_RM_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::FPORT_RM_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::FPORT_RM_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_FPORT_RM_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::FPORT_RM_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::FPORT_RM_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_HILOG_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::HILOG_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::HILOG_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_HILOG_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::HILOG_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::HILOG_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_JPID_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::JPID_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::JPID_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_JPID_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::JPID_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::JPID_FAIL_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_TRACK_JPID_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::TRACK_JPID_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::TRACK_JPID_COUNT)], 1);
}

HWTEST_F(HdcStatisticReporterTest, Test_IncrCommandInfo_TRACK_JPID_FAIL_COUNT, TestSize.Level0)
{
    reporter_.IncrCommandInfo(STATISTIC_ITEM::TRACK_JPID_FAIL_COUNT);
    EXPECT_EQ(reporter_.eventCnt_[static_cast<int>(STATISTIC_ITEM::TRACK_JPID_FAIL_COUNT)], 1);
}
}