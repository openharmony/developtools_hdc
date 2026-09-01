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
#ifndef HDC_HOST_UPDATER_WRAPPER_H
#define HDC_HOST_UPDATER_WRAPPER_H
#include <cstdint>

constexpr uint8_t UPDATER_PERCENT_FINISH = 100;
constexpr uint8_t UPDATER_PERCENT_CLEAR = 255;
constexpr int UPDATER_MAX_RETRY = 3;

struct BeginTransferCfg {
    const char *function;
    int minParam;
    int fileIndex;
};

struct CheckCmdCfg {
    uint16_t command;
    int paramCount;
};

extern "C" {
int HostUpdaterBeginTransfer(const BeginTransferCfg *cfg,
                             const uint8_t *payload, int payloadSize,
                             char *localPath, int maxPathLen);
int HostUpdaterCheckCmd(const CheckCmdCfg *cfg,
                        const uint8_t *payload, int payloadSize,
                        uint8_t *outPayload, int maxOut);
int HostUpdaterConfirmCommand(const char *command, int *closeInput);
int HostUpdaterCheckMatch(const char *input);
void HostUpdaterProcessProgress(uint8_t percentage, uint8_t *sendProgress);
int HostUpdaterValidateFileExt(const char *path);
}

#endif
