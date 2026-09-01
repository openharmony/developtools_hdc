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
#include "async_cmd_wrapper.h"
#include <securec.h>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#else
#include <windows.h>
#include <io.h>
#endif

namespace {
constexpr int MAX_TASKS = 16;
constexpr int READ_BUF_SIZE = 4096;
constexpr int EXIT_EXEC_FAIL = 127;

struct AsyncTask {
    bool active = false;
    std::thread thread;
    std::string output;
    bool done = false;
    bool cancelled = false;
};

AsyncTask g_tasks[MAX_TASKS];
std::mutex g_mutex;
std::atomic<int> g_nextId{0};

int AllocSlot()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!g_tasks[i].active) {
            g_tasks[i].active = true;
            g_tasks[i].done = false;
            g_tasks[i].cancelled = false;
            g_tasks[i].output.clear();
            return i;
        }
    }
    return -1;
}

void CopyOutputToBuffer(int taskId, char *outBuf, int outSize)
{
    if (outBuf == nullptr || outSize <= 0) {
        return;
    }
    int copyLen = static_cast<int>(g_tasks[taskId].output.size());
    if (copyLen > outSize - 1) {
        copyLen = outSize - 1;
    }
    if (memcpy_s(outBuf, outSize, g_tasks[taskId].output.c_str(), copyLen) != EOK) {
        return;
    }
    outBuf[copyLen] = '\0';
}

#ifndef _WIN32
std::string ReadPipeAll(int fd)
{
    std::string result;
    char buf[READ_BUF_SIZE] = {0};
    ssize_t n = 0;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        result += buf;
    }
    return result;
}

void ExecuteUnix(int taskId, const std::string &cmd)
{
    int pipefd[2] = {-1, -1};
    if (pipe(pipefd) < 0) {
        g_tasks[taskId].done = true;
        return;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        g_tasks[taskId].done = true;
        return;
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        _exit(EXIT_EXEC_FAIL);
    }
    close(pipefd[1]);
    std::string result = ReadPipeAll(pipefd[0]);
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (!g_tasks[taskId].cancelled) {
        g_tasks[taskId].output = result;
    }
}
#else
void ExecuteWin(int taskId, const std::string &cmd)
{
    SECURITY_ATTRIBUTES sa = {};
    sa.bInheritHandle = TRUE;
    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    PROCESS_INFORMATION pi = {};
    std::string fullCmd = "cmd /c " + cmd;
    if (CreateProcessA(nullptr, const_cast<char *>(fullCmd.c_str()), nullptr, nullptr,
                       TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWrite);
        char buf[READ_BUF_SIZE] = {0};
        DWORD readBytes = 0;
        std::string result;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &readBytes, nullptr) && readBytes > 0) {
            buf[readBytes] = '\0';
            result += buf;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (!g_tasks[taskId].cancelled) {
            g_tasks[taskId].output = result;
        }
    }
    CloseHandle(hRead);
}
#endif

void ExecuteInternal(int taskId, std::string cmd, uint8_t flags, char *outBuf, int outSize)
{
    if (taskId < 0 || taskId >= MAX_TASKS) {
        return;
    }
#ifndef _WIN32
    ExecuteUnix(taskId, cmd);
#else
    ExecuteWin(taskId, cmd);
#endif
    CopyOutputToBuffer(taskId, outBuf, outSize);
    g_tasks[taskId].done = true;
}
}

extern "C" {
int AsyncCmdExecute(const char *cmd, uint8_t flags, char *outBuf, int outSize)
{
    if (cmd == nullptr) {
        return -1;
    }
    int taskId = AllocSlot();
    if (taskId < 0) {
        return -1;
    }
    bool readback = (flags & ASYNC_CMD_READBACK) != 0;
    if (readback) {
        ExecuteInternal(taskId, std::string(cmd), flags, outBuf, outSize);
    } else {
        g_tasks[taskId].thread = std::thread(ExecuteInternal, taskId,
            std::string(cmd), flags, outBuf, outSize);
        g_tasks[taskId].thread.detach();
    }
    return taskId;
}

void AsyncCmdCancel(int taskId)
{
    if (taskId < 0 || taskId >= MAX_TASKS) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_tasks[taskId].cancelled = true;
}

int AsyncCmdIsRunning(int taskId)
{
    if (taskId < 0 || taskId >= MAX_TASKS) {
        return 0;
    }
    return g_tasks[taskId].active && !g_tasks[taskId].done ? 1 : 0;
}
}
