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
#ifndef USB_UTIL_H
#define USB_UTIL_H

#include <vector>
#include <string>
#include <sys/types.h>
#include <dirent.h>
#include "usb_types.h"
#include <arpa/inet.h>

std::string GetDevPath(const std::string &path);

std::vector<uint8_t> BuildPacketHeader(uint32_t sessionId, uint8_t option, uint32_t data_size);

const std::string StringFormat(const char * const formater, va_list &vaArgs);

const std::string StringFormat(const char * const formater, ...);

bool RunPipeComand(const char *cmdString, char *outBuf, uint16_t sizeOutBuf, bool ignoreTailLf);

bool SetDevItem(const char *key, const char *value);
#endif
