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

#ifndef HDC_SUBSERVER_MANAGER_H
#define HDC_SUBSERVER_MANAGER_H

#include <string>
#include <map>
#include <atomic>
#include <mutex>
#include <memory>

#include "define_plus.h"
#include "subserver_process_info.h"

namespace Hdc {

class HdcServer;

class SubserverManager {
public:
    static SubserverManager& Instance();

    SubserverStatus HandleCommand(HdcServer* server, HChannel channel, const std::string& parameters);

    static void ExitProcess(SubserverStatus status);

    void StartConnectTimer();
    void CancelConnectTimer();
    bool UsbDeviceConnected();

    void NotifyDeviceConnect(const std::string& sn);

    void CheckParentProcess();
    bool CheckClientParam(const std::string& param);

    static void RegisterPid();
    static void UnregisterPid();
    static void KillAllSubservers();
    static std::string GetPidFilePath();

private:
    std::map<std::string, std::shared_ptr<SubserverProcessInfo>> subserverMap;
    std::mutex mapMutex;
    std::shared_ptr<SubserverProcessInfo> GetSubserverProcessInfo(const std::string& serial);

    bool ParseCommandParam(const std::string& parameters, std::string& serial, std::string& port);
    bool DisconnectDevice(HdcServer* server, const std::string& serial);

    SubserverStatus CreateSubserver(const std::string& serial, const std::string& port);

    std::atomic<bool> usbConnected = false;

    bool CheckSerial(const std::string& serial);
    bool CheckPort(const std::string& port);
    bool CheckPurePort(const char* portStr);
    bool CheckIpPort(char* buf, char* colonPos);
    static int ParsePid(const std::string& str);

    SubserverManager() = default;
    ~SubserverManager() = default;
    SubserverManager(const SubserverManager&) = delete;
    SubserverManager& operator=(const SubserverManager&) = delete;
};

} // namespace Hdc

#endif // HDC_SUBSERVER_MANAGER_H