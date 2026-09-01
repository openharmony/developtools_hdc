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
#ifndef HDC_SUBSERVER_WRAPPER_H
#define HDC_SUBSERVER_WRAPPER_H
#include <cstdint>

constexpr int SUBSERVER_STATUS_CONNECTING = 0;
constexpr int SUBSERVER_STATUS_FAIL = 1;
constexpr int SUBSERVER_STATUS_PORT_FAIL = 2;
constexpr int SUBSERVER_STATUS_TIMEOUT = 3;
constexpr int SUBSERVER_STATUS_INVALID = 4;
constexpr int SUBSERVER_STATUS_PARAM_ERR = 5;
constexpr int SUBSERVER_STATUS_SUCCESS = 6;
constexpr int SUBSERVER_STATUS_USB_DISCONN = 7;
constexpr int SUBSERVER_STATUS_ABANDONED = 8;

extern "C" {
int SubserverSpawn(const char *exePath, const char *serial, const char *port,
                   const char *logFile);
int SubserverIsAlive(int pid);
int SubserverGetExitCode(int pid);
void SubserverKillAll();
int SubserverCheckParent();
int SubserverRegisterPid(const char *port, int pid);
int SubserverUnregisterPid(const char *port);
int SubserverKillByPort(const char *port);
int SubserverConnectTimeout(int pid, uint32_t timeoutMs);
}

#endif
