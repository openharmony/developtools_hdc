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
#include "task.h"

namespace Hdc {
// -----------------------------------------------------------
// notice!!! The constructor is called at the Child thread, so in addition to initialization, do not excess actions, if
// destructor is required, please clean up in the subclasses.
HdcTaskBase::HdcTaskBase(HTaskInfo hTaskInfo)
{
    taskInfo = hTaskInfo;
    loopTask = hTaskInfo->runLoop;
    loopTaskStatus = hTaskInfo->runLoopStatus;
    clsSession = hTaskInfo->ownerSessionClass;
    childReady = false;
    singalStop = false;
    refCount = 0;
    if (taskInfo->masterSlave) {
        SendToAnother(CMD_KERNEL_WAKEUP_SLAVETASK, nullptr, 0);
    }
    WRITE_LOG(LOG_DEBUG, "HdcTaskBase type:%u cid:%u sid:%u", taskInfo->taskType, taskInfo->channelId,
        taskInfo->sessionId);
}

HdcTaskBase::~HdcTaskBase()
{
    WRITE_LOG(LOG_DEBUG, "~HdcTaskBase type:%u cid:%u sid:%u", taskInfo->taskType, taskInfo->channelId,
        taskInfo->sessionId);
}

bool HdcTaskBase::ReadyForRelease()
{
    return refCount == 0;
}

// Only the Task work thread call is allowed to use only when Workfortask returns FALSE.
void HdcTaskBase::TaskFinish()
{
    StartTraceScope("HdcTaskBase::TaskFinish");
    uint8_t count = 1;
    WRITE_LOG(LOG_DEBUG, "HdcTaskBase::TaskFinish notify begin type:%u cid:%u sid:%u", taskInfo->taskType,
        taskInfo->channelId, taskInfo->sessionId);
    SendToAnother(CMD_KERNEL_CHANNEL_CLOSE, &count, 1);
    WRITE_LOG(LOG_DEBUG, "HdcTaskBase::TaskFinish notify end type:%u cid:%u sid:%u", taskInfo->taskType,
        taskInfo->channelId, taskInfo->sessionId);
}

bool HdcTaskBase::SendToAnother(const uint16_t command, uint8_t *bufPtr, const int size)
{
    StartTraceScope("HdcTaskBase::SendToAnother");
    if (singalStop) {
        WRITE_LOG(LOG_FATAL, "TaskFinish singalStop channelId:%u command:%u", taskInfo->channelId, command);
        return false;
    }
    if (taskInfo->channelTask) {
        HdcChannelBase *channelBase = reinterpret_cast<HdcChannelBase *>(taskInfo->channelClass);
        channelBase->SendWithCmd(taskInfo->channelId, command, bufPtr, size);
        return true;
    } else {
        HdcSessionBase *sessionBase = reinterpret_cast<HdcSessionBase *>(taskInfo->ownerSessionClass);
        if (Base::IsSessionDeleted(taskInfo->sessionId)) {
            WRITE_LOG(LOG_FATAL, "SendToAnother session is deleted channelId:%u command:%u",
                taskInfo->channelId, command);
            return false;
        }
        return sessionBase->Send(taskInfo->sessionId, taskInfo->channelId, command, bufPtr, size) > 0;
    }
}

void HdcTaskBase::LogMsg(MessageLevel level, const char *msg, ...)
{
    va_list vaArgs;
    va_start(vaArgs, msg);
    string log = Base::StringFormat(msg, vaArgs);
    va_end(vaArgs);

    if (taskInfo->channelTask) {
        string logInfo = "";
        switch (level) {
            case MSG_FAIL:
                logInfo = MESSAGE_FAIL;
                break;
            case MSG_INFO:
                logInfo = MESSAGE_INFO;
                break;
            default:  // successful, not append extra info
                break;
        }

        fprintf(stdout, "%s%s\n", logInfo.c_str(), log.c_str());
        fflush(stdout);
    } else {
        HdcSessionBase *sessionBase = reinterpret_cast<HdcSessionBase *>(clsSession);
        sessionBase->LogMsg(taskInfo->sessionId, taskInfo->channelId, level, log.c_str());
    }
}

bool HdcTaskBase::ServerCommand(const uint16_t command, uint8_t *bufPtr, const int size)
{
    HdcSessionBase *hSession = (HdcSessionBase *)taskInfo->ownerSessionClass;
    return hSession->ServerCommand(taskInfo->sessionId, taskInfo->channelId, command, bufPtr, size);
}

// cross thread
int HdcTaskBase::ThreadCtrlCommunicate(const uint8_t *bufPtr, const int size)
{
    HdcSessionBase *sessionBase = (HdcSessionBase *)taskInfo->ownerSessionClass;
    HSession hSession = sessionBase->AdminSession(OP_QUERY, taskInfo->sessionId, nullptr);
    if (!hSession) {
        WRITE_LOG(LOG_FATAL, "ThreadCtrlCommunicate hSession nullptr sessionId:%u", taskInfo->sessionId);
        return -1;
    }
    int fd = hSession->ctrlFd[STREAM_WORK];
    return Base::SendToPollFd(fd, bufPtr, size);
}
}
