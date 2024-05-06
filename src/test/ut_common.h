/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#ifndef HDC_UT_COMMON_H
#define HDC_UT_COMMON_H
#include "daemon_common.h"
#include "host_common.h"
#include "server.h"

using Hdc::HdcClient;
using Hdc::HdcServer;

#ifndef _WIN32
#include <gtest/gtest.h>
#endif
#include <openssl/md5.h>
#include <stdio.h>
#include <uv.h>

#include "ut_command.h"
#include "ut_mod.h"
#include "ut_runtime.h"

#endif  // end HDC_UT_COMMON_H
