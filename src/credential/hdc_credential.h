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
#ifndef HDI_CREDENTIAL_H
#define HDI_CREDENTIAL_H 

#include <iostream>
#include <sstream>
#include <stdint.h>
#include <string>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "base.h"
#include "common.h"
#include "hdc_huks.h"
#include "log.h"
#include "password.h"

constexpr size_t SOCKET_CLIENT_NUMS = 1;

#endif // HDI_CREDENTIAL_H