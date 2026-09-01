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
#include "host_updater_wrapper.h"
#include <securec.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
const std::string CMD_STR_UPDATE = "update ";
const std::string CMD_STR_FLASH = "flash ";
const std::string CMD_STR_ERASE = "erase ";
const std::string CMD_STR_FORMAT = "format ";

const std::unordered_map<std::string, uint16_t> FLASHD_MAP = {
    {CMD_STR_UPDATE, 4000},
    {CMD_STR_FLASH, 4001},
    {CMD_STR_ERASE, 4006},
    {CMD_STR_FORMAT, 4007},
};

std::vector<std::string> SplitParams(const std::string &src)
{
    std::vector<std::string> result;
    std::string current;
    for (char c : src) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        result.push_back(current);
    }
    return result;
}

bool CheckPathTraversal(const std::string &path)
{
    return path.find("..") == std::string::npos;
}

std::string ExtractCmdParam(const uint8_t *payload, int payloadSize)
{
    std::string cmdParam(reinterpret_cast<const char *>(payload), payloadSize);
    auto nullPos = cmdParam.find('\0');
    if (nullPos != std::string::npos) {
        cmdParam = cmdParam.substr(0, nullPos);
    }
    return cmdParam;
}

bool HasForceFlag(const std::vector<std::string> &params)
{
    for (const auto &p : params) {
        if (p == "-f") {
            return true;
        }
    }
    return false;
}

bool ValidateFilePath(const std::string &path, int maxPathLen)
{
    if (!CheckPathTraversal(path)) {
        return false;
    }
    if (static_cast<int>(path.size()) >= maxPathLen) {
        return false;
    }
    return true;
}
}

extern "C" {
int HostUpdaterBeginTransfer(const BeginTransferCfg *cfg,
                             const uint8_t *payload, int payloadSize,
                             char *localPath, int maxPathLen)
{
    if (cfg == nullptr || cfg->function == nullptr ||
        payload == nullptr || payloadSize <= 0) {
        return -1;
    }
    std::string cmdParam = ExtractCmdParam(payload, payloadSize);
    auto params = SplitParams(cmdParam);

    bool hasForce = HasForceFlag(params);
    int expectedCount = cfg->minParam;
    int actualFileIndex = cfg->fileIndex;
    if (hasForce) {
        expectedCount++;
        actualFileIndex++;
    }
    if (static_cast<int>(params.size()) != expectedCount ||
        static_cast<int>(params.size()) <= actualFileIndex) {
        return -1;
    }

    std::string path = params[actualFileIndex];
    if (!ValidateFilePath(path, maxPathLen)) {
        return -1;
    }
    if (strncpy_s(localPath, maxPathLen, path.c_str(), path.size()) != EOK) {
        return -1;
    }
    localPath[maxPathLen - 1] = '\0';
    return HostUpdaterValidateFileExt(path.c_str());
}

int HostUpdaterCheckCmd(const CheckCmdCfg *cfg,
                        const uint8_t *payload, int payloadSize,
                        uint8_t *outPayload, int maxOut)
{
    if (cfg == nullptr || payload == nullptr ||
        payloadSize <= 0 || outPayload == nullptr) {
        return -1;
    }
    std::string cmdParam = ExtractCmdParam(payload, payloadSize);
    auto params = SplitParams(cmdParam);

    bool hasForce = HasForceFlag(params);
    int expected = hasForce ? cfg->paramCount + 1 : cfg->paramCount;
    if (static_cast<int>(params.size()) != expected) {
        return -1;
    }

    int copyLen = (payloadSize < maxOut) ? payloadSize : maxOut;
    if (memcpy_s(outPayload, maxOut, payload, copyLen) != EOK) {
        return -1;
    }
    return copyLen;
}

void MatchConfirmTip(const std::string &cmd, std::string &tip, int &close)
{
    if (cmd.find(CMD_STR_UPDATE) == 0) {
        close = 1;
    } else if (cmd.find(CMD_STR_FLASH) == 0) {
        tip = "Confirm flash partition";
        close = 1;
    } else if (cmd.find(CMD_STR_ERASE) == 0) {
        tip = "Confirm erase partition";
    } else if (cmd.find(CMD_STR_FORMAT) == 0) {
        tip = "Confirm format partition";
    }
}

std::string ReadUserInput()
{
    char buf[1024] = {0};
    if (fgets(buf, sizeof(buf), stdin) == nullptr) {
        return "";
    }
    std::string input;
    for (char c : buf) {
        if (c == '\r' || c == '\n') {
            break;
        }
        if (c == ' ') {
            continue;
        }
        input += static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
    }
    return input;
}

int ReadUserConfirm(const std::string &tip)
{
    int retryCount = 0;
    do {
        printf("%s ? (Yes/No) ", tip.c_str());
        fflush(stdout);
        std::string input = ReadUserInput();
        if (input.empty()) {
            return 0;
        }
        if (input == "n" || input == "no") {
            return 0;
        }
        if (input == "y" || input == "yes") {
            return 1;
        }
        retryCount++;
    } while (retryCount < UPDATER_MAX_RETRY);
    return 0;
}
}

extern "C" {
int HostUpdaterConfirmCommand(const char *command, int *closeInput)
{
    if (command == nullptr) {
        return 1;
    }
    std::string cmd = command;
    std::string tip;
    int close = 0;
    MatchConfirmTip(cmd, tip, close);

    if (tip.empty() || cmd.find(" -f") != std::string::npos) {
        if (closeInput != nullptr) {
            *closeInput = close;
        }
        return 1;
    }
    if (closeInput != nullptr) {
        *closeInput = close;
    }
    return ReadUserConfirm(tip);
}

int HostUpdaterCheckMatch(const char *input)
{
    if (input == nullptr) {
        return -1;
    }
    std::string str = input;
    for (const auto &pair : FLASHD_MAP) {
        if (str.find(pair.first) == 0 && str.size() > pair.first.size()) {
            return static_cast<int>(pair.second);
        }
    }
    return -1;
}

void HostUpdaterProcessProgress(uint8_t percentage, uint8_t *sendProgress)
{
    if (sendProgress == nullptr || *sendProgress == 0) {
        return;
    }
    if (percentage == UPDATER_PERCENT_CLEAR) {
        printf("\n");
        *sendProgress = 0;
        return;
    }
    printf("\rProcessing:    %d%%", percentage);
    fflush(stdout);
    if (percentage == UPDATER_PERCENT_FINISH) {
        printf("\n");
        *sendProgress = 0;
    }
}

int HostUpdaterValidateFileExt(const char *path)
{
    if (path == nullptr) {
        return -1;
    }
    std::string lower(path);
    for (char &c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    static const std::vector<std::string> validExt = {".img", ".bin", ".fd", ".cpio", ".zip"};
    for (const auto &ext : validExt) {
        if (lower.size() >= ext.size() &&
            lower.compare(lower.size() - ext.size(), ext.size(), ext) == 0) {
            return 1;
        }
    }
    return -1;
}
}
