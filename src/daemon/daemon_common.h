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
#ifndef HDC_DAEMON_COMMON_H
#define HDC_DAEMON_COMMON_H

// clang-format off
#include "common.h"
#include "define.h"
#include "file.h"
#include "forward.h"
#include "async_cmd.h"
#include "serial_struct.h"
#include "tlv.h"

#ifndef HDC_HOST // daemon used
#include "system_depend.h"
#include "jdwp.h"
#include "daemon.h"
#include "daemon_unity.h"
#include "daemon_tcp.h"
#include "daemon_app.h"
#include "daemon_usb.h"
#include "daemon_bridge.h"
#ifdef HDC_SUPPORT_ENCRYPT_TCP
#include "daemon_ssl.h"
#endif
#ifdef HDC_SUPPORT_UART
#include "daemon_uart.h"
#endif
#include "daemon_forward.h"
#include "shell.h"
#endif
// clang-format on

namespace Hdc {
}
#endif