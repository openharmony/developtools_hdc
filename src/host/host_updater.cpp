/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#include "host_updater.h"

#include <algorithm>
#include <unordered_map>

#include "common.h"
#include "define.h"
#include "serial_struct.h"

namespace Hdc {
namespace {
constexpr uint8_t PERCENT_FINISH = 100;
constexpr uint8_t PERCENT_CLEAR = UINT8_MAX;
constexpr int MAX_RETRY_COUNT = 3;
constexpr size_t FLASH_PARAM_MIN_COUNT = 2;
constexpr size_t FLASH_FILE_INDEX = 1;
constexpr size_t UPDATE_PARAM_MIN_COUNT = 1;
constexpr size_t UPDATE_FILE_INDEX = 0;
constexpr size_t FORMAT_PARAM_MIN_COUNT = 2;
constexpr size_t ERASE_PARAM_MIN_COUNT = 2;

const std::string CMD_STR_UPDATE = "update ";
const std::string CMD_STR_FLASH = "flash ";
const std::string CMD_STR_ERASE = "erase ";
const std::string CMD_STR_FORMAT = "format ";

const std::unordered_map<std::string, uint16_t> FLASHD_CMD = {
    {CMD_STR_UPDATE, CMD_FLASHD_UPDATE_INIT},
    {CMD_STR_FLASH, CMD_FLASHD_FLASH_INIT},
    {CMD_STR_ERASE, CMD_FLASHD_ERASE},
    {CMD_STR_FORMAT, CMD_FLASHD_FORMAT},
};

std::vector<std::string> Split(const std::string &src, const std::vector<std::string> &filter)
{
    std::vector<std::string> result;
    if (src.empty()) {
        return result;
    }
    const auto len = src.size() + 1;
    auto buffer = std::vector<char>(len, 0);
    buffer.assign(src.begin(), src.end());
    const char delimit[] = "\t\r\n ";
    char *nextToken = nullptr;
    char *token = strtok_s(buffer.data(), delimit, &nextToken);
    while (token != nullptr) {
        if (std::find(filter.cbegin(), filter.cend(), token) == filter.cend()) {
            result.push_back(token);
        }
        token = strtok_s(nullptr, delimit, &nextToken);
    }
    return result;
}
}

HostUpdater::HostUpdater(HTaskInfo hTaskInfo) : HdcTransferBase(hTaskInfo)
{
    commandBegin = CMD_FLASHD_BEGIN;
    commandData = CMD_FLASHD_DATA;
}

HostUpdater::~HostUpdater() {}

void HostUpdater::RunQueue(CtxFile &context)
{
    refCount++;
    context.localPath = context.taskQueue.back();
    uv_fs_open(loopTask, &context.fsOpenReq, context.localPath.c_str(), O_RDONLY, 0, OnFileOpen);
    context.master = true;
}

bool HostUpdater::BeginTransfer(const std::string &function, const uint8_t *payload, int payloadSize, size_t minParam,
                                size_t fileIndex)
{
    if (payload[payloadSize - 1] != '\0') {
        WRITE_LOG(LOG_FATAL, "payload is invalid");
        return false;
    }
    std::string cmdParam(reinterpret_cast<const char *>(payload));
    auto params = Split(cmdParam, {});
    auto count = minParam;
    auto index = fileIndex;
    if (std::find(params.cbegin(), params.cend(), "-f") != params.cend()) {
        count++;
        index++;
    }
    if (params.size() != count || params.size() <= index) {
        WRITE_LOG(LOG_FATAL, "param count is invalid");
        return false;
    }

    std::string localPath = params[index];
    if (!Base::CheckDirectoryOrPath(localPath.c_str(), true, true)) {
        WRITE_LOG(LOG_FATAL, "localPath is invalid");
        return false;
    }

    if (MatchPackageExtendName(localPath, ".img") || MatchPackageExtendName(localPath, ".bin")) {
        ctxNow.transferConfig.compressType = COMPRESS_NONE;
    } else if (MatchPackageExtendName(localPath, ".zip")) {
        WRITE_LOG(LOG_INFO, "file type is zip");
    } else {
        WRITE_LOG(LOG_FATAL, "file type is invalid");
        return false;
    }
    ctxNow.transferConfig.functionName = function;
    ctxNow.transferConfig.options = cmdParam;
    ctxNow.localPath = localPath;
    ctxNow.taskQueue.push_back(localPath);
    RunQueue(ctxNow);
    return true;
}

void HostUpdater::CheckMaster(CtxFile *context)
{
    uv_fs_t fs;
    Base::ZeroStruct(fs.statbuf);
    uv_fs_fstat(nullptr, &fs, context->fsOpenReq.result, nullptr);
    context->transferConfig.fileSize = fs.statbuf.st_size;
    uv_fs_req_cleanup(&fs);
    context->transferConfig.optionalName = Base::GetFileNameAny(context->localPath);
    std::string bufString = SerialStruct::SerializeToString(context->transferConfig);

    WRITE_LOG(LOG_DEBUG, "functionName = %s, fileSize = %llu", context->transferConfig.functionName.c_str(),
        context->transferConfig.fileSize);

    std::vector<uint8_t> buffer(sizeof(uint64_t) / sizeof(uint8_t), 0);
    buffer.insert(buffer.end(), bufString.begin(), bufString.end());
    SendToAnother(CMD_FLASHD_CHECK, (uint8_t *)buffer.data(), buffer.size());
}

bool HostUpdater::CheckCmd(HdcCommand command, uint8_t *payload, int payloadSize, size_t paramCount)
{
    if (payloadSize < 1 || payload[payloadSize - 1] != '\0') {
        WRITE_LOG(LOG_FATAL, "payload is invalid");
        return false;
    }
    std::string cmdParam(reinterpret_cast<char *>(payload));
    WRITE_LOG(LOG_INFO, "cmdParam = %s, paramCount = %u", cmdParam.c_str(), paramCount);

    auto result = Split(cmdParam, {});
    auto iter = std::find(result.cbegin(), result.cend(), "-f");
    bool ret = (iter != result.cend()) ? (result.size() == (paramCount + 1)) : (result.size() == paramCount);
    if (!ret) {
        WRITE_LOG(LOG_FATAL, "CheckCmd failed");
        return false;
    }

    SendToAnother(command, payload, payloadSize);
    ctxNow.taskQueue.push_back(cmdParam);
    return true;
}

bool HostUpdater::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    if (command == CMD_FLASHD_BEGIN) {
        if (!HdcTransferBase::CommandDispatch(command, payload, payloadSize)) {
            return false;
        }
        std::string tip("Processing:    0%");
        sendProgress_ = true;
        SendRawData(tip);
        return true;
    }

    if (payload == nullptr || payloadSize <= 0) {
        WRITE_LOG(LOG_FATAL, "payload or payloadSize is invalid");
        return false;
    }
    if (!HdcTransferBase::CommandDispatch(command, payload, payloadSize)) {
        return false;
    }
    bool ret = true;
    switch (command) {
        case CMD_FLASHD_UPDATE_INIT:
            ret = BeginTransfer(CMDSTR_FLASHD_UPDATE, payload, payloadSize, UPDATE_PARAM_MIN_COUNT, UPDATE_FILE_INDEX);
            break;
        case CMD_FLASHD_FLASH_INIT:
            ret = BeginTransfer(CMDSTR_FLASHD_FLASH, payload, payloadSize, FLASH_PARAM_MIN_COUNT, FLASH_FILE_INDEX);
            break;
        case CMD_FLASHD_FINISH:
            ret = CheckUpdateContinue(command, payload, payloadSize);
            break;
        case CMD_FLASHD_ERASE:
            ret = CheckCmd(CMD_FLASHD_ERASE, payload, payloadSize, ERASE_PARAM_MIN_COUNT);
            break;
        case CMD_FLASHD_FORMAT:
            ret = CheckCmd(CMD_FLASHD_FORMAT, payload, payloadSize, FORMAT_PARAM_MIN_COUNT);
            break;
        case CMD_FLASHD_PROGRESS:
            ProcessProgress(*payload);
            break;
        default:
            break;
    }
    return ret;
}

void HostUpdater::ProcessProgress(uint8_t percentage)
{
    if (!sendProgress_) {
        return;
    }
    if (percentage == PERCENT_CLEAR) {
        SendRawData("\n");
        sendProgress_ = false;
        return;
    }
    std::string plrogress = "\rProcessing:    " + std::to_string(percentage) + "%";
    SendRawData(plrogress);
    if (percentage == PERCENT_FINISH) {
        SendRawData("\n");
        sendProgress_ = false;
    }
}

bool HostUpdater::CheckUpdateContinue(const uint16_t command, const uint8_t *payload, int payloadSize)
{
    if (static_cast<size_t>(payloadSize) < sizeof(uint16_t)) {
        return false;
    }

    MessageLevel level = static_cast<MessageLevel>(payload[1]);
    if ((level == MSG_OK) && sendProgress_) {
        ProcessProgress(PERCENT_FINISH);
    }
    std::string info(reinterpret_cast<char *>(const_cast<uint8_t *>(payload + sizeof(uint16_t))),
                     payloadSize - sizeof(uint16_t));
    if (!info.empty()) {
        LogMsg(level, "%s", info.c_str());
    }
    WRITE_LOG(LOG_DEBUG, "CheckUpdateContinue payloadSize %d %d %s", payloadSize, level, info.c_str());
    if (ctxNow.taskQueue.size() != 0) {
        ctxNow.taskQueue.pop_back();
    }
    if (singalStop || !ctxNow.taskQueue.size()) {
        return false;
    }
    RunQueue(ctxNow);
    return true;
}

bool HostUpdater::CheckMatchUpdate(const std::string &input, TranslateCommand::FormatCommand &outCmd)
{
    WRITE_LOG(LOG_DEBUG, "CheckMatchUpdate command:%s", input.c_str());
    for (const auto &iter : FLASHD_CMD) {
        if ((input.find(iter.first) == 0) && (input.size() > iter.first.size())) {
            outCmd.cmdFlag = iter.second;
            return true;
        }
    }
    return false;
}

bool HostUpdater::ConfirmCommand(const string &commandIn, bool &closeInput)
{
    std::string tip = "";
    if (!strncmp(commandIn.c_str(), CMD_STR_UPDATE.c_str(), CMD_STR_UPDATE.size())) {
        closeInput = true;
    } else if (!strncmp(commandIn.c_str(), CMD_STR_FLASH.c_str(), CMD_STR_FLASH.size())) {
        tip = "Confirm flash partition";
        closeInput = true;
    } else if (!strncmp(commandIn.c_str(), CMD_STR_ERASE.c_str(), CMD_STR_ERASE.size())) {
        tip = "Confirm erase partition";
    } else if (!strncmp(commandIn.c_str(), CMD_STR_FORMAT.c_str(), CMD_STR_FORMAT.size())) {
        tip = "Confirm format partition";
    }
    if (tip.empty() || strstr(commandIn.c_str(), " -f") != nullptr) {
        return true;
    }
    const size_t minLen = strlen("yes");
    int retryCount = 0;
    do {
        printf("%s ? (Yes/No) ", tip.c_str());
        fflush(stdin);
        std::string info = {};
        size_t i = 0;
        while (1) {
            char c = getchar();
            if (c == '\r' || c == '\n') {
                break;
            }
            if (c == ' ') {
                continue;
            }
            if (i < minLen && isprint(c)) {
                info.append(1, std::tolower(c));
                i++;
            }
        }
        if (info == "n" || info == "no") {
            return false;
        }
        if (info == "y" || info == "yes") {
            return true;
        }
        retryCount++;
    } while (retryCount < MAX_RETRY_COUNT);
    return (retryCount >= MAX_RETRY_COUNT) ? false : true;
}

void HostUpdater::SendRawData(std::string rawData) const
{
    HdcSessionBase *sessionBase = (HdcSessionBase *)clsSession;
    if (sessionBase == nullptr) {
        WRITE_LOG(LOG_FATAL, "sessionBase is null");
        return;
    }
    sessionBase->ServerCommand(taskInfo->sessionId, taskInfo->channelId, CMD_KERNEL_ECHO_RAW,
        reinterpret_cast<uint8_t *>(rawData.data()), rawData.size());
}
} // namespace Hdc