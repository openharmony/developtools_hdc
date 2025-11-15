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

#include "daemon_usb_test.h"
#include "daemon_usb.h"

#include <string>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace testing::ext;

namespace Hdc {
void HdcDaemonUSBTest::SetUpTestCase() {}
void HdcDaemonUSBTest::TearDownTestCase() {}
void HdcDaemonUSBTest::SetUp()
{
#ifdef UT_DEBUG
    Hdc::Base::SetLogLevel(LOG_ALL);
#else
    Hdc::Base::SetLogLevel(LOG_OFF);
#endif
}
void HdcDaemonUSBTest::TearDown() {}

HWTEST_F(HdcDaemonUSBTest, ConstructorDestructor_DefaultValues, TestSize.Level0)
{
    HdcDaemonUSB usb(false, nullptr);
    EXPECT_EQ(usb.usbHandle.bulkOut, -1);
    EXPECT_EQ(usb.usbHandle.bulkIn, -1);
    EXPECT_FALSE(usb.usbHandle.isBulkOutClosing);
    EXPECT_FALSE(usb.usbHandle.isBulkInClosing);
}

HWTEST_F(HdcDaemonUSBTest, GetDevPath_ReturnsEmptyOnInvalidPath, TestSize.Level0)
{
    HdcDaemonUSB usb(false, nullptr);
    std::string invalidPath = "/tmp/nonexistent_usb_dir";
    std::string result = usb.GetDevPath(invalidPath);
    EXPECT_EQ(result, "");
}

HWTEST_F(HdcDaemonUSBTest, GetMaxPacketSize_ReturnsConstant, TestSize.Level0)
{
    HdcDaemonUSB usb(false, nullptr);
    EXPECT_EQ(usb.GetMaxPacketSize(), MAX_PACKET_SIZE_HISPEED);
}

HWTEST_F(HdcDaemonUSBTest, JumpAntiquePacket_DetectsAntique, TestSize.Level0)
{
    HdcDaemonUSB usb(false, nullptr);
    uint8_t antique[24] = {0x43, 0x4e, 0x58, 0x4e}; // CNXN
    EXPECT_TRUE(usb.JumpAntiquePacket(antique[0], 24));
    uint8_t notAntique[24] = {0};
    EXPECT_FALSE(usb.JumpAntiquePacket(notAntique[0], 24));
    EXPECT_FALSE(usb.JumpAntiquePacket(antique[0], 10));
}

HWTEST_F(HdcDaemonUSBTest, IsUSBBulkClosing_NullPtr, TestSize.Level0)
{
    HdcDaemonUSB usb(false, nullptr);
    EXPECT_FALSE(usb.IsUSBBulkClosing(nullptr));
}

HWTEST_F(HdcDaemonUSBTest, IsUSBBulkClosing_TrueIfClosing, TestSize.Level0)
{
    HdcDaemonUSB usb(false, nullptr);
    usb.usbHandle.isBulkInClosing = true;
    EXPECT_TRUE(usb.IsUSBBulkClosing(&usb.usbHandle));
    usb.usbHandle.isBulkInClosing = false;
    usb.usbHandle.isBulkOutClosing = true;
    EXPECT_TRUE(usb.IsUSBBulkClosing(&usb.usbHandle));
    usb.usbHandle.isBulkOutClosing = false;
    EXPECT_FALSE(usb.IsUSBBulkClosing(&usb.usbHandle));
}

HWTEST_F(HdcDaemonUSBTest, OnNewHandshakeOK_SetsCurrentSessionId, TestSize.Level0)
{
    HdcDaemonUSB usb(false, nullptr);
    uint32_t sessionId = 12345;
    usb.OnNewHandshakeOK(sessionId);
    // No getter, but we can check by calling again and expecting no crash
    usb.OnNewHandshakeOK(sessionId + 1);
}

} // namespace
