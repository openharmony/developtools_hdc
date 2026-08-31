/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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
#ifndef HDC_HOST_SHELL_OPT_WRAPPER_H
#define HDC_HOST_SHELL_OPT_WRAPPER_H
#include <cstdint>

constexpr uint32_t SHELL_TAG_CMD = 0x00000000;
constexpr uint32_t SHELL_TAG_BUNDLE = 0x00000001;
constexpr uint32_t SHELL_TAG_DEFAULT = 0xFFFFFFFF;

extern "C" {
int ShellOptFormatToTlv(const char *params, int startPos,
                        uint8_t *out, int maxOut, int *hasCommand);
int ShellOptCheckBundleName(const char *name);
}

#endif
