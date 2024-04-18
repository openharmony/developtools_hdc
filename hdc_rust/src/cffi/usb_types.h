/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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
#ifndef USB_TYPES_H
#define USB_TYPES_H

#include "usb_ffs.h"

struct USBHead {
    uint8_t flag[2];
    uint8_t option;
    uint32_t sessionId;
    uint32_t data_size;
};

struct HdcUSB {
#ifdef HDC_HOST
#else
    // usb accessory FunctionFS
    // USB main thread use, sub-thread disable, sub-thread uses the main thread USB handle
    int bulkOut;  // EP1 device recv
    int bulkIn;   // EP2 device send
#endif
    uint32_t payloadSize;
    uint16_t wMaxPacketSizeSend;
    bool resetIO;  // if true, must break write and read,default false
};

constexpr uint16_t MAX_PACKET_SIZE_HISPEED = 512;
constexpr uint16_t DEVICE_CHECK_INTERVAL = 3000;  // ms
constexpr uint16_t MAX_USBFFS_BULK = 62464;
const std::string USB_PACKET_FLAG = "UB";  // must 2bytes

constexpr int ERR_GENERIC = -1;
constexpr int ERR_SUCCESS = 0;
constexpr int ERR_API_FAIL = -13000;

#endif // USB_TYPES_H
