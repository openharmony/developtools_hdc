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
#include "subserver_wrapper.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#else
#include <windows.h>
#include <tlhelp32.h>
#include <io.h>
#endif

namespace {
constexpr const char *PID_DIR = "/tmp/hdc_subserver_pids/";
constexpr uint32_t DEFAULT_TIMEOUT = 5000;
constexpr int EXIT_EXEC_FAIL = 127;
constexpr int POLL_INTERVAL_MS = 100;

struct ProcessHandle {
    bool valid = false;
    int pid = -1;
    int exitCode = -1;
    std::chrono::steady_clock::time_point startTime;
};

std::mutex g_mutex;
std::map<int, ProcessHandle> g_processes;
std::atomic<int> g_nextId{1};

std::string PidFilePath(const char *port)
{
    return std::string(PID_DIR) + "port_" + (port ? port : "") + ".pid";
}

std::string BuildArgs(const char *serial, const char *port, const char *logFile)
{
    std::ostringstream ss;
    ss << "-N";
    if (serial != nullptr && serial[0] != '\0') {
        ss << " -i " << serial;
    }
    if (port != nullptr && port[0] != '\0') {
        ss << " -o " << port;
    }
    if (logFile != nullptr && logFile[0] != '\0') {
        ss << " -L " << logFile;
    }
    return ss.str();
}

bool WritePidFile(const char *port, int pid)
{
    std::string path = PidFilePath(port);
#ifndef _WIN32
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return false;
    }
    std::string content = std::to_string(pid);
    write(fd, content.c_str(), content.size());
    close(fd);
#else
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << pid;
#endif
    return true;
}

bool ReadPidFile(const char *port, int &outPid)
{
    std::string path = PidFilePath(port);
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return false;
    }
    ifs >> outPid;
    return true;
}

bool RemovePidFile(const char *port)
{
    std::string path = PidFilePath(port);
    return std::remove(path.c_str()) == 0;
}
}

extern "C" {
int SubserverSpawn(const char *exePath, const char *serial, const char *port,
                   const char *logFile)
{
    if (exePath == nullptr) {
        return -1;
    }
    std::string args = BuildArgs(serial, port, logFile);
#ifndef _WIN32
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        execl(exePath, "hdc", args.c_str(), nullptr);
        _exit(EXIT_EXEC_FAIL);
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    int id = g_nextId.fetch_add(1);
    ProcessHandle &ph = g_processes[id];
    ph.valid = true;
    ph.pid = static_cast<int>(pid);
    ph.startTime = std::chrono::steady_clock::now();
    WritePidFile(port, static_cast<int>(pid));
    return id;
#else
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::string cmd = std::string(exePath) + " " + args;
    if (CreateProcessA(nullptr, const_cast<char *>(cmd.c_str()), nullptr, nullptr,
                       FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::lock_guard<std::mutex> lock(g_mutex);
        int id = g_nextId.fetch_add(1);
        ProcessHandle &ph = g_processes[id];
        ph.valid = true;
        ph.pid = static_cast<int>(pi.dwProcessId);
        ph.startTime = std::chrono::steady_clock::now();
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        WritePidFile(port, static_cast<int>(pi.dwProcessId));
        return id;
    }
    return -1;
#endif
}

int SubserverIsAlive(int id)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_processes.find(id);
    if (it == g_processes.end() || !it->second.valid) {
        return 0;
    }
#ifndef _WIN32
    int status = 0;
    pid_t ret = waitpid(it->second.pid, &status, WNOHANG);
    if (ret == it->second.pid || ret < 0) {
        it->second.valid = false;
        it->second.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return 0;
    }
    return 1;
#else
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                           static_cast<DWORD>(it->second.pid));
    if (h == nullptr) {
        it->second.valid = false;
        return 0;
    }
    DWORD code = 0;
    if (GetExitCodeProcess(h, &code) && code != STILL_ACTIVE) {
        it->second.valid = false;
        it->second.exitCode = static_cast<int>(code);
        CloseHandle(h);
        return 0;
    }
    CloseHandle(h);
    return 1;
#endif
}

int SubserverGetExitCode(int id)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_processes.find(id);
    if (it == g_processes.end()) {
        return -1;
    }
    return it->second.exitCode;
}

void SubserverKillAll()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto &pair : g_processes) {
        if (pair.second.valid && pair.second.pid > 0) {
#ifndef _WIN32
            kill(pair.second.pid, SIGTERM);
#endif
            pair.second.valid = false;
        }
    }
    g_processes.clear();
}

int SubserverCheckParent()
{
#ifndef _WIN32
    pid_t ppid = getppid();
    return kill(ppid, 0) == 0 ? 1 : 0;
#else
    return 1;
#endif
}

int SubserverRegisterPid(const char *port, int pid)
{
    return WritePidFile(port, pid) ? 0 : -1;
}

int SubserverUnregisterPid(const char *port)
{
    return RemovePidFile(port) ? 0 : -1;
}

int SubserverKillByPort(const char *port)
{
    int pid = 0;
    if (!ReadPidFile(port, pid) || pid <= 0) {
        return -1;
    }
#ifndef _WIN32
    return kill(pid, SIGTERM) == 0 ? 0 : -1;
#else
    return -1;
#endif
}

int SubserverConnectTimeout(int id, uint32_t timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (SubserverIsAlive(id) == 0) {
            return SUBSERVER_STATUS_FAIL;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    }
    return SUBSERVER_STATUS_SUCCESS;
}
}
