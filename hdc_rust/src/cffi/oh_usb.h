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
#ifndef USB_COMMON_H
#define USB_COMMON_H

#include <vector>
#include <string>

#include "usb_types.h"
#include "usb_ffs.h"

void FillUsbV2Head(struct Hdc::UsbFunctionfsDescV2 &descUsbFfs);

int ConfigEpPoint(int& controlEp, const std::string& path);

int OpenEpPoint(int &fd, const std::string path);

int CloseUsbFd(int &fd);

void CloseEndpoint(int &bulkInFd, int &bulkOutFd, int &controlEp, bool closeCtrlEp);

int WriteData(int bulkIn, const uint8_t *data, const int length);

int ReadData(int bulkOut, uint8_t* buf, const int readMaxSize);

#endif
