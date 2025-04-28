/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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
#include "server_cmd_log.h"
#include <sstream>
#include <unistd.h>
#include "log.h"

namespace Hdc {
ServerCmdLog& ServerCmdLog::GetInstance()
{
    static ServerCmdLog serverCmdLog;
    return serverCmdLog;
}

ServerCmdLog::ServerCmdLog()
{
    lastFlushTime = std::chrono::system_clock::now();
    running_ = true;
}

ServerCmdLog::~ServerCmdLog()
{
}
void ServerCmdLog::PushCmdLogStr(const std::string& cmdLogStr)
{
    std::unique_lock<std::mutex> lock(pushCmdLogStrRecordMutex);
    pushCmdLogStrQueue.push(cmdLogStr);
}

std::string ServerCmdLog::PopCmdLogStr()
{
    std::unique_lock<std::mutex> lock(pushCmdLogStrRecordMutex);
    if (pushCmdLogStrQueue.empty()) {
        return "";
    }
    std::string cmdLogStr = pushCmdLogStrQueue.front();
    pushCmdLogStrQueue.pop();
    lastFlushTime = std::chrono::system_clock::now();
    return cmdLogStr;
}

size_t ServerCmdLog::CmdLogStrSize()
{
    std::unique_lock<std::mutex> lock(pushCmdLogStrRecordMutex);
    return pushCmdLogStrQueue.size();
}

bool ServerCmdLog::GetRunningStatus()
{
    return running_;
}

void ServerCmdLog::SetRunningStatus(bool running)
{
    running_ = running;
}

std::chrono::system_clock::time_point ServerCmdLog::GetLastFlushTime()
{
    return lastFlushTime;
}

} // namespace Hdc