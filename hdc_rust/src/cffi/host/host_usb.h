/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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
#ifndef HDC_HOST_USB_H
#define HDC_HOST_USB_H

#include <memory>
#include <libusb.h>
#include "ctimer.h"
#include <map>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "securec.h"
#include "ffi_utils.h"

namespace Hdc {
using namespace std;
constexpr uint16_t MAX_USBFFS_BULK = 62464;
constexpr uint16_t MAX_USBFFS_BULK2 = 63488;
enum ConnType { CONN_USB = 0, CONN_TCP, CONN_SERIAL, CONN_BT };
enum ConnStatus { STATUS_UNKNOW = 0, STATUS_READY, STATUS_CONNECTED, STATUS_OFFLINE };

enum OperateID {
    OP_ADD,
    OP_REMOVE,
    OP_QUERY,
    OP_QUERY_REF,  // crossthread query, manually reduce ref
    OP_GET_STRLIST,
    OP_GET_STRLIST_FULL,
    OP_GET_READY_STRLIST,
    OP_GET_ANY,
    OP_UPDATE,
    OP_CLEAR,
    OP_INIT,
    OP_GET_ONLY,
    OP_VOTE_RESET,
    OP_WAIT_FOR_ANY
};
struct PersistBuffer {
    char *ptr;
    uint64_t size;
};
struct HdcDaemonInformation {
    uint8_t connType;
    uint8_t connStatus;
    std::string connectKey;
    std::string usbMountPoint;
    std::string devName;
    std::string version;
};
using HDaemonInfo = struct HdcDaemonInformation *;
struct HostUSBEndpoint {
    explicit HostUSBEndpoint(uint16_t epBufSize)
    {
        endpoint = 0;
        sizeEpBuf = epBufSize;  // MAX_USBFFS_BULK
        transfer = libusb_alloc_transfer(0);
        isShutdown = true;
        isComplete = true;
        bulkInOut = false;
        buf = new (std::nothrow) uint8_t[sizeEpBuf];
        (void)memset_s(buf, sizeEpBuf, 0, sizeEpBuf);
    }
    ~HostUSBEndpoint()
    {
        libusb_free_transfer(transfer);
        delete[] buf;
    }
    uint8_t endpoint;
    uint8_t *buf;  // MAX_USBFFS_BULK
    bool isComplete;
    bool isShutdown;
    bool bulkInOut;  // true is bulkIn
    uint16_t sizeEpBuf;
    std::mutex mutexIo;
    std::mutex mutexCb;
    condition_variable cv;
    libusb_transfer *transfer;
};

struct HdcUSB {
    libusb_context *ctxUSB = nullptr;  // child-use, main null
    libusb_device *device;
    libusb_device_handle *devHandle;
    uint16_t retryCount;
    uint8_t devId;
    uint8_t busId;
    uint8_t interfaceNumber;
    std::string serialNumber;
    std::string usbMountPoint;
    HostUSBEndpoint hostBulkIn;
    HostUSBEndpoint hostBulkOut;
    HdcUSB() : hostBulkIn(MAX_USBFFS_BULK2), hostBulkOut(MAX_USBFFS_BULK) {}

    uint32_t payloadSize;
    uint16_t wMaxPacketSizeSend;
    bool resetIO;  // if true, must break write and read,default false
    std::mutex lockDeviceHandle;
    std::mutex lockSendUsbBlock;
};
using HUSB = struct HdcUSB *;

enum UsbCheckStatus {
    HOST_USB_IGNORE = 1,
    HOST_USB_READY,
    HOST_USB_REGISTER,
};

class HostUsb {
public:
    HostUsb();
    ~HostUsb();
    int Initial();
    void Stop();
    static void UsbWorkThread(void *arg);  // 3rd thread
    static void WatchUsbNodeChange(void *arg);
    static void LIBUSB_CALL USBBulkCallback(struct libusb_transfer *transfer);
    void CancelUsbIo(HUSB hUsb);
    PersistBuffer ReadUsbIO(HUSB hUsb, int exceptedSize);
    int WriteUsbIO(HUSB hUsb, SerializedBuffer buf);
    HUSB GetUsbDevice(std::string connectKey);
    string AdminDaemonMap(uint8_t opType, const string &connectKey, HDaemonInfo &hDaemonInfoInOut);
    void UpdateUSBDaemonInfo(HUSB hUSB, uint8_t connStatus);
private:
    bool DetectMyNeed(libusb_device *device, string &sn);
    void ReviewUsbNodeLater(string &nodeKey);
    void RemoveIgnoreDevice(string &mountInfo);
    void CheckUsbEndpoint(int& ret, HUSB hUSB, libusb_config_descriptor *descConfig);
#ifdef HOST_MAC
    int CheckActiveConfig(libusb_device *device, HUSB hUSB, libusb_device_descriptor& desc);
#else
    int CheckActiveConfig(libusb_device *device, HUSB hUSB);
#endif
    int OpenDeviceMyNeed(HUSB hUSB);
    bool HasValidDevice(libusb_device *device);
    int CheckDescriptor(HUSB hUSB, libusb_device_descriptor& desc);
    bool IsDebuggableDev(const struct libusb_interface_descriptor *ifDescriptor);
    bool FindDeviceByID(HUSB hUSB, const char *usbMountPoint, libusb_context *ctxUSB);
    string GetDaemonMapList(uint8_t opType);
    
    libusb_context *ctxUSB;
    bool running;
    std::map<string, UsbCheckStatus> mapIgnoreDevice;
    std::unique_ptr<CTimer> timer;
    std::map<string, HUSB> mapUsbDevice;
    std::mutex lockMapDaemon;
    map<string, HDaemonInfo> mapDaemon;
};
}
#endif
