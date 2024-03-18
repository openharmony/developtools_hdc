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
#include "oh_usb.h"
#include "usb_ffs.h"
#include "usb_util.h"
#include "sys_para.h"
#include "log.h"

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

static constexpr int CONFIG_COUNT2 = 2;
static constexpr int CONFIG_COUNT3 = 3;
static constexpr int CONFIG_COUNT5 = 5;

// make gnuc++ happy. Clang support direct assignment value to structure, buf g++ weakness
void FillUsbV2Head(struct Hdc::UsbFunctionfsDescV2 &descUsbFfs)
{
    descUsbFfs.head.magic = LONG_LE(FUNCTIONFS_DESCRIPTORS_MAGIC_V2);
    descUsbFfs.head.length = LONG_LE(sizeof(descUsbFfs));
    descUsbFfs.head.flags = FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC |
                            FUNCTIONFS_HAS_SS_DESC | FUNCTIONFS_HAS_MS_OS_DESC;
    descUsbFfs.config1Count = CONFIG_COUNT3;
    descUsbFfs.config2Count = CONFIG_COUNT3;
    descUsbFfs.config3Count = CONFIG_COUNT5;
    descUsbFfs.configWosCount = CONFIG_COUNT2;
    descUsbFfs.config1Desc = Hdc::config1;
    descUsbFfs.config2Desc = Hdc::config2;
    descUsbFfs.config3Desc = Hdc::config3;
    descUsbFfs.wosHead = Hdc::g_wosHead;
    descUsbFfs.wosDesc = Hdc::g_wosDesc;
    descUsbFfs.osPropHead = Hdc::g_osPropHead;
    descUsbFfs.osPropValues = Hdc::g_osPropValues;
}

int ConfigEpPoint(int& controlEp, const std::string& path)
{
    struct Hdc::UsbFunctionfsDescV2 descUsbFfs = {};
    FillUsbV2Head(descUsbFfs);
    while (true) {
        if (controlEp <= 0) {
            // After the control port sends the instruction, the device is initialized by the device to the HOST host,
            // which can be found for USB devices. Do not send initialization to the EP0 control port, the USB
            // device will not be initialized by Host
            printf("Begin send to control(EP0) for usb descriptor init\n");
            if ((controlEp = open(path.c_str(), O_RDWR)) < 0) {
                printf("%s: cannot open control endpoint: errno=%d\n", path.c_str(), errno);
                break;
            }
            if (write(controlEp, &descUsbFfs, sizeof(descUsbFfs)) < 0) {
                printf("%s: write ffs configs failed: errno=%d\n", path.c_str(), errno);
                break;
            }
            if (write(controlEp, &Hdc::USB_FFS_VALUE, sizeof(Hdc::USB_FFS_VALUE)) < 0) {
                printf("%s: write USB_FFS_VALUE failed: errno=%d\n", path.c_str(), errno);
                break;
            }
            // active usbrc, Send USB initialization signal
            printf("ConnectEPPoint ctrl init finish, set usb-ffs ready\n");
            fcntl(controlEp, F_SETFD, FD_CLOEXEC);
            Hdc::SetDevItem("sys.usb.ffs.ready.hdc", "0");
            Hdc::SetDevItem("sys.usb.ffs.ready", "1");
            Hdc::SetDevItem("sys.usb.ffs.ready.hdc", "1");
            return ERR_SUCCESS;
        }
    }
    return ERR_GENERIC;
}

int OpenEpPoint(int &fd, const std::string path)
{
    if ((fd = open(path.c_str(), O_RDWR)) < 0) {
        printf("%s: cannot open ep: errno=%d\n", path.c_str(), errno);
        return ERR_GENERIC;
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    return ERR_SUCCESS;
}

int CloseUsbFd(int &fd)
{
        int rc = 0;
#ifndef HDC_HOST
        printf("CloseFd fd:%d\n", fd);
#endif
        if (fd > 0) {
            rc = close(fd);
            if (rc < 0) {
                char buffer[BUF_SIZE_DEFAULT] = { 0 };
#ifdef _WIN32
                strerror_s(buffer, BUF_SIZE_DEFAULT, errno);
#else
                strerror_r(errno, buffer, BUF_SIZE_DEFAULT);
#endif
                printf("close failed errno:%d %s\n", errno, buffer);
            } else {
                fd = -1;
            }
        }
        return rc;
}

void CloseEndpoint(int &bulkInFd, int &bulkOutFd, int &controlEp, bool closeCtrlEp)
{
    CloseUsbFd(bulkInFd);
    CloseUsbFd(bulkOutFd);
    if (controlEp > 0 && closeCtrlEp) {
        CloseUsbFd(controlEp);
        controlEp = 0;
    }
    printf("close endpoint ok\n");
}

int WriteData(int bulkIn, const uint8_t *data, const int length)
{
    int ret;
    int retry_times = 10;
    // 10ms
    int retry_interval = 10000;

    while (retry_times > 0) {
        ret = write(bulkIn, const_cast<uint8_t *>(data), length);
        if (ret >= 0)
            break;

        if (errno != EINTR)
            break;

        usleep(retry_interval);
        retry_times--;
    }

    return ret;
}

int ReadData(int bulkOut, uint8_t* buf, const int readMaxSize)
{
    int ret;
    int retry_times = 10;
    // 10ms
    int retry_interval = 10000;

    while (retry_times > 0) {
        ret = read(bulkOut, buf, readMaxSize);
        if (ret >= 0)
            break;

        if (errno != EINTR)
            break;

        usleep(retry_interval);
        retry_times--;
    }

    return ret;
}
