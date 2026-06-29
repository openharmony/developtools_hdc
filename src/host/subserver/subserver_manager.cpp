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

#include "subserver_manager.h"

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <sstream>
#include <thread>

#include "file_lock_util.h"
#include "process_handle.h"
#include "../host_common.h"
#include "../host_usb.h"
#include "../runtime_config.h"
#include "../server.h"
#include "../server_for_client.h"

namespace Hdc {

SubserverManager& SubserverManager::Instance()
{
    static SubserverManager instance;
    return instance;
}

std::shared_ptr<SubserverProcessInfo> SubserverManager::GetSubserverProcessInfo(const std::string& serial)
{
    auto it = subserverMap.find(serial);
    if (it == subserverMap.end()) {
        return nullptr;
    }
    return it->second;
}

SubserverStatus SubserverManager::HandleCommand(HdcServer* server, HChannel channel, const std::string& parameters)
{
    if (RuntimeConfig::Instance().isSubserver) {
        return SubserverStatus::SUBSERVER_ABONDON;
    }
    std::unique_lock<std::mutex> lock(mapMutex);
    std::string serial;
    std::string port;
    if (!ParseCommandParam(parameters, serial, port)) {
        return SubserverStatus::PARAM_ERROR;
    }

    std::shared_ptr<SubserverProcessInfo> subserverProcessInfo = GetSubserverProcessInfo(serial);
    if (subserverProcessInfo != nullptr) {
        SubserverStatus status = subserverProcessInfo->GetSubserverStatus();
        if (status != SubserverStatus::CONNECTING) {
            // if status is not connecting, cleanup state and prepare for next connection.
            subserverMap.erase(serial);
        }
        return status;
    }

    // disconnect USB connection so that subserver can connect to the device.
    if (!DisconnectDevice(server, serial)) {
        return SubserverStatus::INVALID_DEVICE;
    }

    return CreateSubserver(serial, port);
}

bool SubserverManager::ParseCommandParam(const std::string& parameters, std::string& serial, std::string& port)
{
    auto findStringFunc = [parameters](const std::string& opt, std::string& out, bool& duplicate) -> bool {
        size_t pos = parameters.find(opt);
        if (pos == std::string::npos) {
            return false;
        }
        pos += opt.size();
        while (pos < parameters.size() && std::isspace(static_cast<unsigned char>(parameters[pos]))) {
            ++pos;
        }
        if (pos == parameters.size()) {
            return false;
        }

        size_t start = pos;
        while (pos < parameters.size()) {
            if (std::isspace(static_cast<unsigned char>(parameters[pos]))) {
                break;
            }
            if (parameters[pos] == '-' &&
                (pos == start || std::isspace(static_cast<unsigned char>(parameters[pos - 1])))) {
                break;
            }
            ++pos;
        }

        out = parameters.substr(start, pos - start);

        size_t nextPos = parameters.find(opt, pos);
        if (nextPos != std::string::npos) {
            duplicate = true;
        }
        return true;
    };

    bool duplicateI = false;
    bool duplicateO = false;
    bool findI = findStringFunc("-i", serial, duplicateI);
    bool findO = findStringFunc("-o", port, duplicateO);
    return findI && findO && !duplicateI && !duplicateO;
}

bool SubserverManager::DisconnectDevice(HdcServer* server, const std::string& serial)
{
    if (server == nullptr) {
        return false;
    }
    HDaemonInfo daemonInfo = nullptr;
    server->AdminDaemonMap(OP_QUERY, serial.c_str(), daemonInfo);
    if (daemonInfo == nullptr || daemonInfo->hSession == nullptr || daemonInfo->hSession->hUSB == nullptr) {
        return false;
    }

    std::string szTmpKey = Base::StringFormat("%d-%d",
                                              libusb_get_bus_number(daemonInfo->hSession->hUSB->device),
                                              libusb_get_device_address(daemonInfo->hSession->hUSB->device));

    server->clsUSBClt->ReviewUsbNodeLater(szTmpKey, HdcHostUSB::UsbCheckStatus::HOST_USB_SUSPENDED);

    server->FreeSession(daemonInfo->hSession->sessionId);
    return true;
}

void SubserverManager::CheckParentProcess()
{
    std::string parentName = ProcessHandle::GetParentProcessName();
    if (parentName == "hdc" || parentName.find("hdc.exe") != std::string::npos) {
        return;
    }
    Base::PrintMessage("Error: -N is reserved for internal use");
    fflush(stdout);
    std::_Exit(static_cast<int>(0));
}

bool SubserverManager::CheckClientParam(const std::string& param)
{
    std::string serial;
    std::string port;
    if (!ParseCommandParam(param, serial, port)) {
        Base::PrintMessage("spawn-sub command must have correct option of '-i' and '-o'");
        return false;
    }

    return CheckSerial(serial) && CheckPort(port);
}

bool SubserverManager::CheckSerial(const std::string& serial)
{
    uint32_t len = serial.size();
    if (len > MAX_CONNECTKEY_SIZE) {
        Base::PrintMessage("Size of parament '-i' %u is too long", len);
        return false;
    }
    return true;
}

bool SubserverManager::CheckPurePort(const char* portStr)
{
    if (strlen(portStr) > PORT_MAX_LEN) {
        Base::PrintMessage("The port-string's length must <= 5");
        return false;
    }
    size_t len = strlen(portStr);
    for (size_t i = 0; i < len; i++) {
        if (isdigit(portStr[i]) == 0) {
            Base::PrintMessage("The port must be digit buf:%s", portStr);
            return false;
        }
    }
    int portNum = atoi(portStr);
    if (portNum <= 0 || portNum > MAX_IP_PORT) {
        Base::PrintMessage("Port range incorrect");
        return false;
    }
    return true;
}

bool SubserverManager::CheckIpPort(char* buf, char* colonPos)
{
    *colonPos = '\0';
    char* portStr = colonPos + 1;

    size_t len = strlen(portStr);
    for (size_t i = 0; i < len; i++) {
        if (isdigit(portStr[i]) == 0) {
            Base::PrintMessage("The port must be digit str:%s", portStr);
            return false;
        }
    }
    int portNum = atoi(portStr);
    if (portNum <= 0 || portNum > MAX_IP_PORT) {
        Base::PrintMessage("-o content port incorrect.");
        return false;
    }

    sockaddr_in addrv4;
    sockaddr_in6 addrv6;
    if (uv_ip4_addr(buf, portNum, &addrv4) != 0 &&
        uv_ip6_addr(buf, portNum, &addrv6) != 0) {
        Base::PrintMessage("-o content IP incorrect.");
        return false;
    }
    return true;
}

bool SubserverManager::CheckPort(const std::string& port)
{
    if (port.size() > strlen("0000::0000:0000:0000:0000%interfacename:65535")) {
        Base::PrintMessage("Unknown content of parament '-o'");
        return false;
    }
    char buf[BUF_SIZE_TINY] = "";
    if (strcpy_s(buf, sizeof(buf), port.c_str()) != 0) {
        Base::PrintMessage("strcpy_s error %d", errno);
        return false;
    }

    char* p = strrchr(buf, ':');
    if (!p) {
        return CheckPurePort(buf);
    }
    return CheckIpPort(buf, p);
}

int SubserverManager::ParsePid(const std::string& str)
{
    char* endPtr = nullptr;
    errno = 0;
    long pidLong = strtol(str.c_str(), &endPtr, 10);

    if (endPtr == str.c_str() || *endPtr != '\0') {
#ifdef HDC_DEBUG
        WRITE_LOG(LOG_DEBUG, "Invalid PID format, str: %s", str.c_str());
#endif
        return -1;
    }

    if (errno == ERANGE) {
#ifdef HDC_DEBUG
        WRITE_LOG(LOG_DEBUG, "PID overflow, str: %s", str.c_str());
#endif
        return -1;
    }

    if (pidLong <= 0) {
#ifdef HDC_DEBUG
        WRITE_LOG(LOG_DEBUG, "Invalid PID value: %ld", pidLong);
#endif
        return -1;
    }

    if (pidLong > INT_MAX) {
#ifdef HDC_DEBUG
        WRITE_LOG(LOG_DEBUG, "PID exceeds INT_MAX: %ld", pidLong);
#endif
        return -1;
    }

    return static_cast<int>(pidLong);
}

SubserverStatus SubserverManager::CreateSubserver(const std::string& serial, const std::string& port)
{
    std::string runPath = ProcessHandle::GetExecutablePath();
    char args[BUF_SIZE_SMALL] = "";

    if (!ProcessHandle::BuildSubserverArgs(args, sizeof(args), port.c_str(), serial.c_str(), port.c_str())) {
        return SubserverStatus::PARAM_ERROR;
    }

    auto processHandle = ProcessHandle::SpawnSubprocess(runPath, args);
    if (!processHandle) {
        return SubserverStatus::SUBPROCESS_FAIL;
    }

    auto subserverProcessInfo = std::make_shared<SubserverProcessInfo>(std::move(processHandle));
    subserverMap[serial] = subserverProcessInfo;

    return SubserverStatus::CONNECTING;
}

void SubserverManager::ExitProcess(SubserverStatus status)
{
    UnregisterPid();
    fflush(stdout);
    std::_Exit(static_cast<int>(status));
}

void SubserverManager::StartConnectTimer()
{
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(SUBSERVER_CONNECT_TIMEOUT));
        if (!SubserverManager::Instance().usbConnected) {
            ExitProcess(SubserverStatus::CONNECT_TIMEOUT);
        }
    }).detach();
}

void SubserverManager::CancelConnectTimer()
{
    usbConnected = true;
}

bool SubserverManager::UsbDeviceConnected()
{
    return usbConnected;
}

std::string SubserverManager::GetPidFilePath()
{
    char buf[BUF_SIZE_DEFAULT] = "";
    size_t size = sizeof(buf);
#ifdef HOST_OHOS
    if (uv_os_homedir(buf, &size) < 0) {
        WRITE_LOG(LOG_WARN, "Get homedir failed");
        return "";
    }
#else
    if (uv_os_tmpdir(buf, &size) < 0) {
        WRITE_LOG(LOG_WARN, "Get tmpdir failed");
        return "";
    }
#endif
    std::string path = std::string(buf) + Base::GetPathSep() + ".HDC_Subserver.pid";
    return path;
}

void SubserverManager::RegisterPid()
{
    std::string path = GetPidFilePath();
    if (path.empty()) {
        WRITE_LOG(LOG_WARN, "GetPidFilePath failed");
        return;
    }
    std::string content = std::to_string(ProcessHandle::GetCurrentPid()) + "\n";
    FileLockUtil::AtomicAppend(path, content);
}

void SubserverManager::UnregisterPid()
{
    std::string path = GetPidFilePath();
    if (path.empty()) {
        WRITE_LOG(LOG_WARN, "GetPidFilePath failed");
        return;
    }
    std::string lineToRemove = std::to_string(ProcessHandle::GetCurrentPid());
    FileLockUtil::AtomicRemoveLine(path, lineToRemove);
}

void SubserverManager::KillAllSubservers()
{
    std::string path = GetPidFilePath();
    if (path.empty()) {
        WRITE_LOG(LOG_WARN, "GetPidFilePath failed");
        return;
    }

    std::string content;
    if (!FileLockUtil::AtomicReadAndClear(path, content)) {
        WRITE_LOG(LOG_WARN, "ReadAndClear failed");
        return;
    }

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        int pid = ParsePid(line);
        if (pid <= 0) {
            continue;
        }

        int rc = uv_kill(pid, SIGKILL);
        if (rc != 0) {
#ifdef HDC_DEBUG
            WRITE_LOG(LOG_DEBUG, "uv_kill failed, pid: %d, rc: %d", pid, rc);
#else
            WRITE_LOG(LOG_DEBUG, "uv_kill failed, rc: %d", rc);
#endif
        }
    }
}
} // namespace Hdc
