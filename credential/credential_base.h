/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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
#ifndef HDC_CREDENTIAL_BASE_H
#define HDC_CREDENTIAL_BASE_H

#include <chrono>
#include <dirent.h>
#include <securec.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include "log.h"

#ifdef HDC_HILOG
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif // LOG_DOMAIN

#define LOG_DOMAIN 0xD002D13
#ifdef LOG_TAG
#undef LOG_TAG
#endif // LOG_TAG
#define LOG_TAG "HDC_LOG"
#endif // HDC_HILOG

// 0x10000000 is 1.0.0a
constexpr uint32_t CREDENTIAL_VERSION_NUMBER = 0x10000000;
constexpr size_t SOCKET_CLIENT_NUMS = 1;

#define HDC_PRIVATE_KEY_FILE_PWD_KEY_ALIAS "hdc_private_key_file_pwd_key_alias"
constexpr size_t PASSWORD_LENGTH = 10;

constexpr uint32_t MAX_SIZE_IOBUF_STABLE = 60 * 1024; // 60KB, compatible with previous version
const std::string HDC_CREDENTIAL_SOCKET_REAL_PATH =
    "/data/service/el1/public/hdc_server/hdc_common/hdc_credential.socket";
constexpr uint8_t CMD_ARG1_COUNT = 2;

int RemoveDir(const std::string& dir);
int RemovePath(const std::string& path);
const std::string StringFormat(const char* const formater, ...);
const std::string StringFormat(const char* const formater, va_list& vaArgs);
char GetPathSep();

#endif // HDC_CREDENTIAL_BASE_H