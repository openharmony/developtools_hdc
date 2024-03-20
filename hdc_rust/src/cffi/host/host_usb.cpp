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
#include "host_usb.h"

#include <thread>
#include "usb_util.h"

namespace Hdc {
constexpr uint16_t DEVICE_CHECK_INTERVAL = 3000;  // ms
constexpr uint16_t BUF_SIZE_MEDIUM = 512;
constexpr uint16_t BUF_SIZE_TINY = 64;
constexpr uint8_t GLOBAL_TIMEOUT = 30;
constexpr uint16_t TIME_BASE = 1000;
constexpr uint16_t MAX_SIZE_IOBUF = 61440;

uint8_t *g_bufRet = nullptr;

const std::string StringFormat(const char * const formater, va_list &vaArgs)
{
    std::vector<char> args(MAX_SIZE_IOBUF);
    const int retSize = vsnprintf_s(args.data(), MAX_SIZE_IOBUF, MAX_SIZE_IOBUF - 1, formater, vaArgs);
    if (retSize < 0) {
        return std::string("");
    } else {
        return std::string(args.data(), retSize);
    }
}

const std::string StringFormat(const char * const formater, ...)
{
    va_list vaArgs;
    va_start(vaArgs, formater);
    std::string ret = StringFormat(formater, vaArgs);
    va_end(vaArgs);
    return ret;
}

HostUsb::HostUsb()
{
    if (libusb_init((libusb_context **)&ctxUSB) != 0) {
        ctxUSB = nullptr;
    }
    running = false;
}

HostUsb::~HostUsb()
{
    if (running) {
        Stop();
    }
}

void HostUsb::Stop()
{
    if (!ctxUSB) {
        return;
    }
    timer->Stop();
    libusb_exit((libusb_context *)ctxUSB);
    running = false;
}

// Main thread USB operates in this thread
void HostUsb::UsbWorkThread(void *arg)
{
    HostUsb *thisClass = (HostUsb *)arg;
    constexpr uint8_t usbHandleTimeout = 30;  // second
    while (thisClass->running) {
        struct timeval zerotime;
        zerotime.tv_sec = usbHandleTimeout;
        zerotime.tv_usec = 0;  // if == 0,windows will be high CPU load
        libusb_handle_events_timeout(thisClass->ctxUSB, &zerotime);
    }
}

void HostUsb::WatchUsbNodeChange(void *arg)
{
    HostUsb *thisClass = (HostUsb *)arg;
    libusb_device **devs = nullptr;
    libusb_device *dev = nullptr;
    ssize_t cnt = libusb_get_device_list(thisClass->ctxUSB, &devs);
    if (cnt < 0) {
        return;
    }
    int i = 0;
    // linux replug devid increment，windows will be not
    while ((dev = devs[i++]) != nullptr) {  // must postfix++
        std::string szTmpKey = StringFormat("%d-%d", libusb_get_bus_number(dev), libusb_get_device_address(dev));
        // check is in ignore list
        UsbCheckStatus statusCheck = thisClass->mapIgnoreDevice[szTmpKey];
        if (statusCheck == HOST_USB_IGNORE || statusCheck == HOST_USB_REGISTER) {
            continue;
        }
        std::string sn = szTmpKey;
        if (thisClass->HasValidDevice(dev) && !thisClass->DetectMyNeed(dev, sn)) {
            thisClass->ReviewUsbNodeLater(szTmpKey);
        }
    }
    libusb_free_device_list(devs, 1);
}

void HostUsb::ReviewUsbNodeLater(string &nodeKey)
{
    // add to ignore list
    mapIgnoreDevice[nodeKey] = HOST_USB_IGNORE;
    RemoveIgnoreDevice(nodeKey);
}

bool HostUsb::HasValidDevice(libusb_device *device)
{
    struct libusb_config_descriptor *descConfig = nullptr;
    int ret = libusb_get_active_config_descriptor(device, &descConfig);
    if (ret != 0) {
        return false;
    }
    bool hasValid = false;
    for (unsigned int j = 0; j < descConfig->bNumInterfaces; ++j) {
        const struct libusb_interface *interface = &descConfig->interface[j];
        if (interface->num_altsetting < 1) {
            continue;
        }
        const struct libusb_interface_descriptor *ifDescriptor = &interface->altsetting[0];
        if (!IsDebuggableDev(ifDescriptor)) {
            continue;
        }
        hasValid = true;
        break;
    }
    return hasValid;
}

bool HostUsb::IsDebuggableDev(const struct libusb_interface_descriptor *ifDescriptor)
{
    constexpr uint8_t harmonyEpNum = 2;
    constexpr uint8_t harmonyClass = 0xff;
    constexpr uint8_t harmonySubClass = 0x50;
    constexpr uint8_t harmonyProtocol = 0x01;

    if (ifDescriptor->bInterfaceClass != harmonyClass || ifDescriptor->bInterfaceSubClass != harmonySubClass ||
        ifDescriptor->bInterfaceProtocol != harmonyProtocol) {
        return false;
    }
    if (ifDescriptor->bNumEndpoints != harmonyEpNum) {
        return false;
    }
    return true;
}

bool HostUsb::DetectMyNeed(libusb_device *device, string &sn)
{
    HUSB hUSB = new(std::nothrow) HdcUSB();
    if (hUSB == nullptr) {
        return false;
    }
    hUSB->device = device;
    // just get usb SN, close handle immediately
    int childRet = OpenDeviceMyNeed(hUSB);
    if (childRet < 0) {
        delete hUSB;
        return false;
    }
    UpdateUSBDaemonInfo(hUSB, STATUS_READY);
    mapIgnoreDevice[sn] = HOST_USB_REGISTER;
    mapUsbDevice[hUSB->serialNumber] = hUSB;
    return true;
}

void HostUsb::UpdateUSBDaemonInfo(HUSB hUSB, uint8_t connStatus)
{
    // add to list
    HdcDaemonInformation di;
    di.connectKey = hUSB->serialNumber;
    di.connType = CONN_USB;
    di.connStatus = connStatus;
    di.usbMountPoint = "";
    di.usbMountPoint = StringFormat("%d-%d", hUSB->busId, hUSB->devId);

    HDaemonInfo pDi = nullptr;
    HDaemonInfo hdiNew = &di;
    AdminDaemonMap(OP_QUERY, hUSB->serialNumber, pDi);
    if (!pDi) {
        AdminDaemonMap(OP_ADD, hUSB->serialNumber, hdiNew);
    } else {
        AdminDaemonMap(OP_UPDATE, hUSB->serialNumber, hdiNew);
        if (connStatus == STATUS_OFFLINE) {
            RemoveIgnoreDevice(di.usbMountPoint);
        }
    }
}

// ==0 Represents new equipment and is what we need,<0  my need
int HostUsb::OpenDeviceMyNeed(HUSB hUSB)
{
    libusb_device *device = hUSB->device;
    int ret = -1;
    int openRet = libusb_open(device, &hUSB->devHandle);
    if (openRet != LIBUSB_SUCCESS) {
        return -1;
    }
    while (running) {
        libusb_device_handle *handle = hUSB->devHandle;
        struct libusb_device_descriptor desc;
        if (CheckDescriptor(hUSB, desc)) {
            break;
        }
#ifdef HOST_MAC
        if (CheckActiveConfig(device, hUSB, desc)) {
#else
        if (CheckActiveConfig(device, hUSB)) {
#endif
            break;
        }
        
        // USB filter rules are set according to specific device pedding device
        ret = libusb_claim_interface(handle, hUSB->interfaceNumber);
        break;
    }
    if (ret) {
        // not my need device, release the device
        libusb_close(hUSB->devHandle);
        hUSB->devHandle = nullptr;
    }
    return ret;
}

int HostUsb::CheckDescriptor(HUSB hUSB, libusb_device_descriptor& desc)
{
    char serialNum[BUF_SIZE_MEDIUM] = "";
    int childRet = 0;
    uint8_t curBus = libusb_get_bus_number(hUSB->device);
    uint8_t curDev = libusb_get_device_address(hUSB->device);
    hUSB->busId = curBus;
    hUSB->devId = curDev;
    if (libusb_get_device_descriptor(hUSB->device, &desc)) {
        return -1;
    }
    // Get the serial number of the device, if there is no serial number, use the ID number to replace
    // If the device is not in time, occasionally can't get it, this is determined by the external factor, cannot be
    // changed. LIBUSB_SUCCESS
    childRet = libusb_get_string_descriptor_ascii(hUSB->devHandle, desc.iSerialNumber, (uint8_t *)serialNum,
                                                  sizeof(serialNum));
    if (childRet < 0) {
        return -1;
    } else {
        hUSB->serialNumber = serialNum;
    }
    return 0;
}

#ifdef HOST_MAC
int HostUsb::CheckActiveConfig(libusb_device *device, HUSB hUSB, libusb_device_descriptor& desc)
#else
int HostUsb::CheckActiveConfig(libusb_device *device, HUSB hUSB)
#endif
{
    struct libusb_config_descriptor *descConfig = nullptr;
    int ret = libusb_get_active_config_descriptor(device, &descConfig);
    if (ret != 0) {
#ifdef HOST_MAC
        if ((desc.bDeviceClass == 0xFF)
            && (desc.bDeviceSubClass == 0xFF)
            && (desc.bDeviceProtocol == 0xFF)) {
            ret = libusb_set_configuration(hUSB->devHandle, 1);
            if (ret != 0) {
                return -1;
            }
        }

        ret = libusb_get_active_config_descriptor(device, &descConfig);
        if (ret != 0) {
#endif
            return -1;
        }
#ifdef HOST_MAC
    }
#endif

    ret = -1;
    CheckUsbEndpoint(ret, hUSB, descConfig);
    libusb_free_config_descriptor(descConfig);
    return ret;
}

void HostUsb::CheckUsbEndpoint(int& ret, HUSB hUSB, libusb_config_descriptor *descConfig)
{
    unsigned int j = 0;
    for (j = 0; j < descConfig->bNumInterfaces; ++j) {
        const struct libusb_interface *interface = &descConfig->interface[j];
        if (interface->num_altsetting < 1) {
            continue;
        }
        const struct libusb_interface_descriptor *ifDescriptor = &interface->altsetting[0];
        if (!IsDebuggableDev(ifDescriptor)) {
            continue;
        }
        hUSB->interfaceNumber = ifDescriptor->bInterfaceNumber;
        unsigned int k = 0;
        for (k = 0; k < ifDescriptor->bNumEndpoints; ++k) {
            const struct libusb_endpoint_descriptor *ep_desc = &ifDescriptor->endpoint[k];
            if ((ep_desc->bmAttributes & 0x03) != LIBUSB_TRANSFER_TYPE_BULK) {
                continue;
            }
            if (ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
                hUSB->hostBulkIn.endpoint = ep_desc->bEndpointAddress;
                hUSB->hostBulkIn.bulkInOut = true;
            } else {
                hUSB->hostBulkOut.endpoint = ep_desc->bEndpointAddress;
                hUSB->wMaxPacketSizeSend = ep_desc->wMaxPacketSize;
                hUSB->hostBulkOut.bulkInOut = false;
            }
        }
        if (hUSB->hostBulkIn.endpoint == 0 || hUSB->hostBulkOut.endpoint == 0) {
            break;
        }
        ret = 0;
    }
}

bool HostUsb::FindDeviceByID(HUSB hUSB, const char *usbMountPoint, libusb_context *ctxUSB)
{
    libusb_device **listDevices = nullptr;
    bool ret = false;
    char tmpStr[BUF_SIZE_TINY] = "";
    int busNum = 0;
    int devNum = 0;
    int curBus = 0;
    int curDev = 0;

    int deviceNum = libusb_get_device_list(ctxUSB, &listDevices);
    if (deviceNum <= 0) {
        libusb_free_device_list(listDevices, 1);
        return false;
    }
    if (strchr(usbMountPoint, '-') && EOK == strcpy_s(tmpStr, sizeof(tmpStr), usbMountPoint)) {
        *strchr(tmpStr, '-') = '\0';
        busNum = atoi(tmpStr);
        devNum = atoi(tmpStr + strlen(tmpStr) + 1);
    } else {
        return false;
    }

    int i = 0;
    for (i = 0; i < deviceNum; ++i) {
        struct libusb_device_descriptor desc;
        if (LIBUSB_SUCCESS != libusb_get_device_descriptor(listDevices[i], &desc)) {
            continue;
        }
        curBus = libusb_get_bus_number(listDevices[i]);
        curDev = libusb_get_device_address(listDevices[i]);
        if ((curBus == busNum && curDev == devNum)) {
            hUSB->device = listDevices[i];
            int childRet = OpenDeviceMyNeed(hUSB);
            if (!childRet) {
                ret = true;
            } else {
                string key = string(usbMountPoint);
                RemoveIgnoreDevice(key);
            }
            break;
        }
    }
    libusb_free_device_list(listDevices, 1);
    return ret;
}

// multi-thread calll
void HostUsb::CancelUsbIo(HUSB hUSB)
{
    std::unique_lock<std::mutex> lock(hUSB->lockDeviceHandle);
    if (!hUSB->hostBulkIn.isShutdown) {
        if (!hUSB->hostBulkIn.isComplete) {
            libusb_cancel_transfer(hUSB->hostBulkIn.transfer);
            hUSB->hostBulkIn.cv.notify_one();
        } else {
            hUSB->hostBulkIn.isShutdown = true;
        }
    }
    if (!hUSB->hostBulkOut.isShutdown) {
        if (!hUSB->hostBulkOut.isComplete) {
            libusb_cancel_transfer(hUSB->hostBulkOut.transfer);
            hUSB->hostBulkOut.cv.notify_one();
        } else {
            hUSB->hostBulkOut.isShutdown = true;
        }
    }
}

void HostUsb::RemoveIgnoreDevice(string &mountInfo)
{
    if (mapIgnoreDevice.count(mountInfo)) {
        mapIgnoreDevice.erase(mountInfo);
    }
}

void LIBUSB_CALL HostUsb::USBBulkCallback(struct libusb_transfer *transfer)
{
    auto *ep = reinterpret_cast<HostUSBEndpoint *>(transfer->user_data);
    std::unique_lock<std::mutex> lock(ep->mutexIo);
    bool retrySumit = false;
    int childRet = 0;
    do {
        if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            break;
        }
        if (!ep->bulkInOut && transfer->actual_length != transfer->length) {
            transfer->length -= transfer->actual_length;
            transfer->buffer += transfer->actual_length;
            retrySumit = true;
            break;
        }
    } while (false);
    while (retrySumit) {
        childRet = libusb_submit_transfer(transfer);
        if (childRet != 0) {
            transfer->status = LIBUSB_TRANSFER_ERROR;
            break;
        }
        return;
    }
    ep->isComplete = true;
    ep->cv.notify_one();
}

PersistBuffer HostUsb::ReadUsbIO(HUSB hUSB, int exceptedSize)
{
    int timeout = 0;
    int childRet = 0;
    int ret = 0;

    HostUSBEndpoint* ep = &hUSB->hostBulkIn;

    if (g_bufRet == nullptr) {
        printf("remalloc g_bufRet\n");
        g_bufRet = new uint8_t[MAX_SIZE_IOBUF];
    }

    hUSB->lockDeviceHandle.lock();
    ep->isComplete = false;
    do {
        std::unique_lock<std::mutex> lock(ep->mutexIo);
        libusb_fill_bulk_transfer(ep->transfer, hUSB->devHandle, ep->endpoint, g_bufRet, exceptedSize,
            USBBulkCallback, ep, timeout);
        childRet = libusb_submit_transfer(ep->transfer);
        hUSB->lockDeviceHandle.unlock();
        if (childRet < 0) {
            break;
        }
        ep->cv.wait(lock, [ep]() { return ep->isComplete; });
        if (ep->transfer->status != 0) {
            break;
        }
        ret = ep->transfer->actual_length;
    } while (false);
    return PersistBuffer{reinterpret_cast<char *>(g_bufRet), static_cast<uint64_t>(ret)};
}

HUSB HostUsb::GetUsbDevice(std::string connectKey)
{
    return mapUsbDevice[connectKey];
}

int HostUsb::WriteUsbIO(HUSB hUSB, SerializedBuffer buf)
{
    int childRet = 0;
    int ret = -14000;
    int timeout = GLOBAL_TIMEOUT * TIME_BASE;
    HostUSBEndpoint *ep = &hUSB->hostBulkOut;

    hUSB->lockDeviceHandle.lock();
    ep->isComplete = false;
    uint8_t* ptr = reinterpret_cast<uint8_t *>(buf.ptr);
    size_t size = static_cast<size_t>(buf.size);
    do {
        std::unique_lock<std::mutex> lock(ep->mutexIo);
        libusb_fill_bulk_transfer(ep->transfer, hUSB->devHandle, ep->endpoint, ptr, size, USBBulkCallback, ep,
                                  timeout);
        childRet = libusb_submit_transfer(ep->transfer);
        hUSB->lockDeviceHandle.unlock();
        if (childRet < 0) {
            break;
        }
        ep->cv.wait(lock, [ep]() { return ep->isComplete; });
        if (ep->transfer->status != 0) {
            break;
        }
        ret = ep->transfer->actual_length;
    } while (false);
    return ret;
}

int HostUsb::Initial()
{
    if (!ctxUSB) {
        return -1;
    }
    running = true;
    auto WatchUsbNodeChangeFunc = [this]() { WatchUsbNodeChange(this); };
    timer = std::make_unique<CTimer>(WatchUsbNodeChangeFunc);
    timer->Start(DEVICE_CHECK_INTERVAL);
    std::thread([this]() {
        UsbWorkThread(this);
    }).detach();
    return 0;
}

static void BuildDaemonVisableLine(HDaemonInfo hdi, bool fullDisplay, string &out)
{
    if (fullDisplay) {
        string sConn;
        string sStatus;
        switch (hdi->connType) {
            case CONN_TCP:
                sConn = "TCP";
                break;
            case CONN_USB:
                sConn = "USB";
                break;
#ifdef HDC_SUPPORT_UART
            case CONN_SERIAL:
                sConn = "UART";
                break;
#endif
            case CONN_BT:
                sConn = "BT";
                break;
            default:
                sConn = "UNKNOW";
                break;
        }
        switch (hdi->connStatus) {
            case STATUS_READY:
                sStatus = "Ready";
                break;
            case STATUS_CONNECTED:
                sStatus = "Connected";
                break;
            case STATUS_OFFLINE:
                sStatus = "Offline";
                break;
            default:
                sStatus = "UNKNOW";
                break;
        }
        out = StringFormat("%s\t\t%s\t%s\t%s\n", hdi->connectKey.c_str(), sConn.c_str(), sStatus.c_str(),
                                 hdi->devName.c_str());
    } else {
        if (hdi->connStatus == STATUS_CONNECTED) {
            out = StringFormat("%s\n", hdi->connectKey.c_str());
        }
    }
}

string HostUsb::GetDaemonMapList(uint8_t opType)
{
    string ret;
    bool fullDisplay = false;
    if (opType == OP_GET_STRLIST_FULL) {
        fullDisplay = true;
    }
    lockMapDaemon.lock();
    map<string, HDaemonInfo>::iterator iter;
    string echoLine;
    for (iter = mapDaemon.begin(); iter != mapDaemon.end(); ++iter) {
        HDaemonInfo di = iter->second;
        if (!di) {
            continue;
        }
        echoLine = "";
        if (opType == OP_GET_READY_STRLIST) {
            if (di->connStatus == STATUS_READY) {
                echoLine = StringFormat("%s ", di->connectKey.c_str());
                ret += echoLine;
            }
            continue;
        }
        BuildDaemonVisableLine(di, fullDisplay, echoLine);
        ret += echoLine;
    }
    lockMapDaemon.unlock();
    return ret;
}

string HostUsb::AdminDaemonMap(uint8_t opType, const string &connectKey, HDaemonInfo &hDaemonInfoInOut)
{
    string sRet;
    switch (opType) {
        case OP_ADD: {
            HDaemonInfo pdiNew = new(std::nothrow) HdcDaemonInformation();
            if (pdiNew == nullptr) {
                break;
            }
            *pdiNew = *hDaemonInfoInOut;
            lockMapDaemon.lock();
            if (!mapDaemon[hDaemonInfoInOut->connectKey]) {
                mapDaemon[hDaemonInfoInOut->connectKey] = pdiNew;
            }
            lockMapDaemon.unlock();
            break;
        }
        case OP_GET_READY_STRLIST:
            sRet = GetDaemonMapList(opType);
            break;
        case OP_GET_STRLIST:
        case OP_GET_STRLIST_FULL: {
            sRet = GetDaemonMapList(opType);
            break;
        }
        case OP_QUERY: {
            lockMapDaemon.lock();
            if (mapDaemon.count(connectKey)) {
                hDaemonInfoInOut = mapDaemon[connectKey];
            }
            lockMapDaemon.unlock();
            break;
        }
        case OP_REMOVE: {
            lockMapDaemon.lock();
            if (mapDaemon.count(connectKey)) {
                mapDaemon.erase(connectKey);
            }
            lockMapDaemon.unlock();
            break;
        }
        case OP_GET_ANY: {
            lockMapDaemon.lock();
            map<string, HDaemonInfo>::iterator iter;
            for (iter = mapDaemon.begin(); iter != mapDaemon.end(); ++iter) {
                HDaemonInfo di = iter->second;
                // usb will be auto connected
                if (di->connStatus == STATUS_READY || di->connStatus == STATUS_CONNECTED) {
                    hDaemonInfoInOut = di;
                    break;
                }
            }
            lockMapDaemon.unlock();
            break;
        }
        case OP_WAIT_FOR_ANY: {
            lockMapDaemon.lock();
            map<string, HDaemonInfo>::iterator iter;
            for (iter = mapDaemon.begin(); iter != mapDaemon.end(); ++iter) {
                HDaemonInfo di = iter->second;
                if (di->connStatus == STATUS_CONNECTED) {
                    hDaemonInfoInOut = di;
                    break;
                }
            }
            lockMapDaemon.unlock();
            break;
        }
        case OP_UPDATE: {  // Cannot update the Object HDi lower key value by direct value
            lockMapDaemon.lock();
            HDaemonInfo hdi = mapDaemon[hDaemonInfoInOut->connectKey];
            if (hdi) {
                *mapDaemon[hDaemonInfoInOut->connectKey] = *hDaemonInfoInOut;
            }
            lockMapDaemon.unlock();
            break;
        }
        default:
            break;
    }
    return sRet;
}
}