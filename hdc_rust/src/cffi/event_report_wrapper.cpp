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
#include "event_report_wrapper.h"
#include <securec.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#else
#include <winsock2.h>
#include <afunix.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace {
constexpr const char *SOCKET_PATH = "/data/hdc/hdc_event_report";
std::mutex g_mutex;

std::string FormatEvent(uint16_t type, const char *role, const char *status, const char *detail)
{
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    const char *typeName = "unknown";
    switch (type) {
        case EVENT_CMD: typeName = "command"; break;
        case EVENT_FILE: typeName = "file"; break;
        case EVENT_CONN: typeName = "connection"; break;
        default: break;
    }
    std::ostringstream ss;
    ss << ts << "|" << typeName << "|" << (role ? role : "")
       << "|" << (status ? status : "")
       << "|" << (detail ? detail : "");
    return ss.str();
}

bool SendToUnixSocket(const std::string &msg)
{
#ifndef _WIN32
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        return false;
    }
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    if (strncpy_s(addr.sun_path, sizeof(addr.sun_path), SOCKET_PATH,
                   sizeof(addr.sun_path) - 1) != EOK) {
        close(fd);
        return false;
    }
    bool ok = sendto(fd, msg.c_str(), msg.size(), 0,
                     reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) > 0;
    close(fd);
    return ok;
#else
    return false;
#endif
}
}

extern "C" {
void EventReport(uint16_t type, const char *role, const char *status, const char *detail)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    std::string msg = FormatEvent(type, role, status, detail);
    SendToUnixSocket(msg);
}
}
