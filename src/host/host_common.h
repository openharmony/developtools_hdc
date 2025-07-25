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
#ifndef HDC_HOST_COMMON_H
#define HDC_HOST_COMMON_H

// clang-format off
#include "common.h"
#include "file.h"
#include "transfer.h"
#include "forward.h"
#include "async_cmd.h"
#include "serial_struct.h"
#include "tlv.h"

#include "host_tcp.h"
#include "host_usb.h"
#ifdef HDC_SUPPORT_ENCRYPT_TCP
#include "host_ssl.h"
#endif
#ifdef HDC_SUPPORT_UART
#include "host_uart.h"
#endif
#include "translate.h"
#include "server_for_client.h"
#include "client.h"
#include "host_app.h"
#include "host_forward.h"
#include "host_unity.h"
#include "host_shell_option.h"
// clang-format on

#endif
