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
    std::string invalidPath = "/data/local/tmp/nonexistent_usb_dir";
    if (std::filesystem::exists(invalidPath))
    {
        std::filesystem::remove(invalidPath);
    }
    ASSERT_FALSE(std::filesystem::exists(invalidPath));
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
    EXPECT_EQ(usb.currentSessionId, sessionId);
    // No getter, but we can check by calling again and expecting no crash
    usb.OnNewHandshakeOK(sessionId + 1);
    EXPECT_EQ(usb.currentSessionId, sessionId + 1);
}

static vector<uint8_t> BuildPacketHeaderOld(uint32_t sessionId, uint8_t option, uint32_t dataSize)
{
    const string USB_PACKET_FLAG = "UB";  // must 2bytes
    vector<uint8_t> vecData;
    USBHead head;
    head.sessionId = htonl(sessionId);
    for (size_t i = 0; i < sizeof(head.flag); i++) {
        head.flag[i] = USB_PACKET_FLAG.data()[i];
    }
    head.option = option;
    head.dataSize = htonl(dataSize);
    vecData.insert(vecData.end(), (uint8_t *)&head, (uint8_t *)&head + sizeof(USBHead));
    return vecData;
}

HWTEST_F(HdcDaemonUSBTest, BuildPacketHeaderTest, TestSize.Level0)
{
    static constexpr uint32_t sessionId = 13245;
    static constexpr uint8_t option = 7;
    static constexpr uint32_t dataSize = 15;
    vector<uint8_t> vec = BuildPacketHeaderOld(sessionId, option, dataSize);
    HdcDaemonUSB usb(false, nullptr);
    USBHead* head = usb.BuildPacketHeader(sessionId, option, dataSize);

    EXPECT_EQ(vec.size(), sizeof(USBHead));
    uint8_t* p1 = vec.data();
    uint8_t* p2 = reinterpret_cast<uint8_t*>(head);
    for (int i = 0; i < vec.size(); i++) {
        EXPECT_EQ(*(p1 + i), *(p2 + i));
    }
    delete head;
}

} // namespace
