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

#include "server_instance.h"

#include <cstdint>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "base.h"
#include "define.h"

namespace Hdc {
namespace {
constexpr size_t INSTANCE_INFO_MAX_SIZE = 256;
constexpr char INSTANCE_INFO_SEPARATOR = '|';
constexpr char UDS_ENDPOINT[] = "uds";

std::string GetInstancePath(const char *suffix)
{
    const std::string directory = Base::GetTmpDir();
    if (directory.empty()) {
        return "";
    }
    return Base::StringFormat("%s.%s.%s", directory.c_str(), SERVER_NAME.c_str(), suffix);
}

bool ReadInstanceFile(const std::string &path, std::string &content)
{
    if (path.empty()) {
        return false;
    }
    char buffer[INSTANCE_INFO_MAX_SIZE] = {};
    uv_fs_t request = {};
    int fd = uv_fs_open(nullptr, &request, path.c_str(), O_RDONLY, 0, nullptr);
    uv_fs_req_cleanup(&request);
    if (fd < 0) {
        return false;
    }
    uv_buf_t data = uv_buf_init(buffer, sizeof(buffer));
    int size = uv_fs_read(nullptr, &request, fd, &data, 1, 0, nullptr);
    uv_fs_req_cleanup(&request);
    uv_fs_close(nullptr, &request, fd, nullptr);
    uv_fs_req_cleanup(&request);
    if (size <= 0 || size >= static_cast<int>(sizeof(buffer))) {
        return false;
    }
    content.assign(buffer, static_cast<size_t>(size));
    return true;
}

bool ParseInstanceInfo(const std::string &content, uint32_t &pid, std::string &endpoint)
{
    size_t separator = content.find(INSTANCE_INFO_SEPARATOR);
    if (separator == std::string::npos || content.find(INSTANCE_INFO_SEPARATOR, separator + 1) != std::string::npos) {
        return false;
    }
    int parsedPid = 0;
    if (!Base::StringToInt(content.substr(0, separator), parsedPid) || parsedPid <= 0) {
        return false;
    }
    endpoint = content.substr(separator + 1);
    if (endpoint.empty() || endpoint.find_first_of("\r\n") != std::string::npos) {
        return false;
    }
    pid = static_cast<uint32_t>(parsedPid);
    return true;
}

bool IsEndpointConflict(const std::string &requested, const std::string &active)
{
    if (requested == active) {
        return false;
    }
    if (requested == UDS_ENDPOINT) {
        return true;
    }
    char requestedIp[BUF_SIZE_TINY] = {};
    char activeIp[BUF_SIZE_TINY] = {};
    uint16_t requestedPort = 0;
    uint16_t activePort = 0;
    if (Base::ConnectKey2IPPort(requested.c_str(), requestedIp, &requestedPort, sizeof(requestedIp)) != RET_SUCCESS) {
        return false;
    }
    bool loopback = strcmp(requestedIp, "::1") == 0 || strncmp(requestedIp, "127.", 4) == 0 ||
        strncmp(requestedIp, "::ffff:127.", 11) == 0;
    if (active == UDS_ENDPOINT) {
        return loopback;
    }
    if (Base::ConnectKey2IPPort(active.c_str(), activeIp, &activePort, sizeof(activeIp)) != RET_SUCCESS) {
        return false;
    }
    return (loopback || strcmp(requestedIp, activeIp) == 0) && requestedPort != activePort;
}
} // namespace

bool WriteServerInstanceInfo(const std::string &endpoint)
{
    const std::string path = GetInstancePath("info");
    if (path.empty() || endpoint.empty()) {
        return false;
    }
#ifdef _WIN32
    const uint32_t pid = static_cast<uint32_t>(_getpid());
#else
    const uint32_t pid = static_cast<uint32_t>(getpid());
#endif
    const std::string content = std::to_string(pid) + "|" + endpoint;
    const std::string temporaryPath = path + ".tmp";
    uv_fs_t request = {};
    int fd = uv_fs_open(nullptr, &request, temporaryPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                        S_IRUSR | S_IWUSR, nullptr);
    uv_fs_req_cleanup(&request);
    if (fd < 0) {
        return false;
    }
    uv_buf_t data = uv_buf_init(const_cast<char *>(content.data()), content.size());
    int written = uv_fs_write(nullptr, &request, fd, &data, 1, 0, nullptr);
    uv_fs_req_cleanup(&request);
    int closeStatus = uv_fs_close(nullptr, &request, fd, nullptr);
    uv_fs_req_cleanup(&request);
    if (written != static_cast<int>(content.size()) || closeStatus < 0) {
        uv_fs_unlink(nullptr, &request, temporaryPath.c_str(), nullptr);
        uv_fs_req_cleanup(&request);
        return false;
    }
    int status = uv_fs_rename(nullptr, &request, temporaryPath.c_str(), path.c_str(), nullptr);
    uv_fs_req_cleanup(&request);
    if (status < 0) {
        uv_fs_unlink(nullptr, &request, temporaryPath.c_str(), nullptr);
        uv_fs_req_cleanup(&request);
        return false;
    }
    return true;
}

std::string InspectServerInstance(const std::string &requestedEndpoint)
{
    if (Base::ProgramMutex(true) != 1) {
        return {};
    }
    std::string infoContent;
    std::string pidContent;
    uint32_t infoPid = 0;
    int activePid = 0;
    std::string activeEndpoint;
    if (!ReadInstanceFile(GetInstancePath("info"), infoContent) ||
        !ReadInstanceFile(GetInstancePath("pid"), pidContent) ||
        !ParseInstanceInfo(infoContent, infoPid, activeEndpoint) ||
        !Base::StringToInt(pidContent, activePid) || activePid <= 0 || infoPid != static_cast<uint32_t>(activePid)) {
        return {};
    }

    if (!IsEndpointConflict(requestedEndpoint, activeEndpoint)) {
        return {};
    }
    return Base::StringFormat(
        "[E002116] HDC server endpoint conflict: requested %s, active %s. Use \"-s %s\" or stop the active server.",
        requestedEndpoint.c_str(), activeEndpoint.c_str(), activeEndpoint.c_str());
}
} // namespace Hdc
