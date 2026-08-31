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
#ifndef HDC_ASYNC_CMD_WRAPPER_H
#define HDC_ASYNC_CMD_WRAPPER_H
#include <cstdint>

constexpr uint8_t ASYNC_CMD_READBACK = 1;
constexpr uint8_t ASYNC_CMD_ONETIME = 2;

extern "C" {
int AsyncCmdExecute(const char *cmd, uint8_t flags, char *outBuf, int outSize);
void AsyncCmdCancel(int taskId);
int AsyncCmdIsRunning(int taskId);
}

#endif
