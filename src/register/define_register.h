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

#ifndef HDC_DEFINE_REGISTER_H
#define HDC_DEFINE_REGISTER_H

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include <securec.h>
#include <sys/un.h>
#include <unistd.h>
#include <uv.h>
#ifdef HDC_HILOG
#include "hilog/log.h"
#endif

namespace Hdc {
static constexpr OHOS::HiviewDFX::HiLogLabel LOG_LABEL = {LOG_CORE, 0xD002D13, "HDC_LOG"};
// str one of ark:pid@com.xxx.xxxx, ark:pid@Debugger, ark:pid@tid@Debugger
using Callback = std::function<void(int fd, std::string str)>;
}
#endif // end HDC_DEFINE_REGISTER_H
