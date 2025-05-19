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
#ifndef HDC_CMD_LOG_H
#define HDC_CMD_LOG_H
#include <mutex>
#include <queue>
#include <string>

namespace Hdc {
class ServerCmdLog {
public:
    static ServerCmdLog& GetInstance();
    ServerCmdLog();
    ~ServerCmdLog();
    void PushCmdLogStr(const std::string& cmdLogStr);
    std::string PopCmdLogStr();
    size_t CmdLogStrSize();
    std::chrono::system_clock::time_point GetLastFlushTime();

private:
    std::chrono::system_clock::time_point lastFlushTime = std::chrono::system_clock::now();
    std::mutex pushCmdLogStrRecordMutex;
    std::queue<std::string> pushCmdLogStrQueue;
};
} // namespace Hdc
#endif // HDC_CMD_LOG_H