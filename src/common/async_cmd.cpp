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
#include "async_cmd.h"
#include <pthread.h>
#if !defined(_WIN32) && !defined(HDC_HOST)
#include "parameter.h"
#include "base.h"
#if defined(SURPPORT_SELINUX)
#include "selinux/selinux.h"
#endif
#endif

namespace Hdc {
// Do not add thread-specific init op in the following methods as it's running in child thread.
AsyncCmd::AsyncCmd()
{
}

AsyncCmd::~AsyncCmd()
{
    if (childShell != nullptr) {
        delete childShell;
        childShell = nullptr;
    }
    WRITE_LOG(LOG_DEBUG, "~AsyncCmd");
};

bool AsyncCmd::ReadyForRelease()
{
    if (childShell != nullptr && !childShell->ReadyForRelease()) {
        return false;
    }
    if (refCount != 0) {
        return false;
    }
    if (childShell != nullptr) {
        delete childShell;
        childShell = nullptr;
    }
    Base::CloseFd(fd);
    return true;
}

void AsyncCmd::DoRelease()
{
    WRITE_LOG(LOG_DEBUG, "AsyncCmd::DoRelease finish");
    if (childShell != nullptr) {
        childShell->StopWorkOnThread(false, nullptr);
    }
    if (pid > 0) {
        uv_kill(pid, SIGTERM);
    }
}

bool AsyncCmd::Initial(uv_loop_t *loopIn, const CmdResultCallback callback, uint32_t optionsIn)
{
#if defined _WIN32 || defined HDC_HOST
    WRITE_LOG(LOG_FATAL, "Not support for win32 or host side");
    return false;
#endif
    loop = loopIn;
    resultCallback = callback;
    options = optionsIn;
    return true;
}

bool AsyncCmd::FinishShellProc(const void *context, const bool result, const string exitMsg)
{
    AsyncCmd *thisClass = static_cast<AsyncCmd *>(const_cast<void *>(context));
    WRITE_LOG(LOG_DEBUG, "FinishShellProc finish pipeRead fd:%d pid:%d", thisClass->fd, thisClass->pid);
    thisClass->resultCallback(true, result, thisClass->cmdResult + exitMsg);
    --thisClass->refCount;
    return true;
};

bool AsyncCmd::ChildReadCallback(const void *context, uint8_t *buf, const int size)
{
    AsyncCmd *thisClass = static_cast<AsyncCmd *>(const_cast<void *>(context));
    if (thisClass->options & OPTION_COMMAND_ONETIME) {
        string s(reinterpret_cast<char *>(buf), size);
        thisClass->cmdResult += s;
        return true;
    }
    string s(reinterpret_cast<char *>(buf), size);
    return thisClass->resultCallback(false, 0, s);
};

#if !defined(_WIN32) && !defined(HDC_HOST)
bool AsyncCmd::GetDevItem(const char *key, string &out)
{
    bool ret = true;
    char tmpStringBuf[BUF_SIZE_MEDIUM] = "";
#ifdef HARMONY_PROJECT
    auto res = GetParameter(key, nullptr, tmpStringBuf, BUF_SIZE_MEDIUM);
    if (res <= 0) {
        return false;
    }
#else
    string sFailString = Base::StringFormat("Get parameter \"%s\" fail", key);
    string stringBuf = "param get " + string(key);
    Base::RunPipeComand(stringBuf.c_str(), tmpStringBuf, BUF_SIZE_MEDIUM - 1, true);
    if (!strcmp(sFailString.c_str(), tmpStringBuf)) {
        // failed
        ret = false;
        Base::ZeroArray(tmpStringBuf);
    }
#endif
    out = tmpStringBuf;
    return ret;
}

static void SetSelinuxLabel(bool isRoot)
{
#if defined(SURPPORT_SELINUX)
    char *con = nullptr;
    if (getcon(&con) != 0) {
        return;
    }
    if ((strcmp(con, "u:r:hdcd:s0") != 0) && (strcmp(con, "u:r:updater:s0") != 0)) {
        freecon(con);
        return;
    }
    WRITE_LOG(LOG_DEBUG, "async shell mode:%d", isRoot);
#ifdef HDC_BUILD_VARIANT_USER
    setcon("u:r:sh:s0");
#else
    if (isRoot) {
        setcon("u:r:su:s0");
    } else {
        setcon("u:r:sh:s0");
    }
#endif
    freecon(con);
#endif
}
#endif

int AsyncCmd::ThreadFork(const string &command, bool readWrite, int &cpid)
{
    string debugMode = "";
    string rootMode = "";
    bool isRoot = false;
#if !defined(_WIN32) && !defined(HDC_HOST)
    GetDevItem("const.debuggable", debugMode);
    GetDevItem("persist.hdc.root", rootMode);
#endif
    if (debugMode == "1" && rootMode == "1") {
        isRoot = true;
    }
    AsyncParams params = AsyncParams(command, readWrite, cpid, isRoot);
    pthread_t threadId;
    void *popenRes;
    int ret = pthread_create(&threadId, nullptr, reinterpret_cast<void *(*)(void *)>(Popen), &params);
    if (ret != 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
#ifdef _WIN32
        strerror_s(buf, bufSize, errno);
#else
        strerror_r(errno, buf, bufSize);
#endif
        WRITE_LOG(LOG_DEBUG, "fork Thread create failed:%s", buf);
        return ERR_GENERIC;
    }
    pthread_join(threadId, &popenRes);
    return static_cast<int>(reinterpret_cast<size_t>(popenRes));
}

void *AsyncCmd::Popen(void *arg)
{
#ifdef _WIN32
    return reinterpret_cast<void *>(ERR_NO_SUPPORT);
#else
#ifndef HOST_MAC
    int ret = pthread_setname_np(pthread_self(), "hdcd_popen");
    if (ret != 0) {
        WRITE_LOG(LOG_DEBUG, "set Thread name failed.");
    }
#else
    int ret = pthread_setname_np("hdcd_popen");
    if (ret != 0) {
        WRITE_LOG(LOG_DEBUG, "set Thread name failed.");
    }
#endif  
    AsyncParams params = *reinterpret_cast<AsyncParams *>(arg);
    string command = params.commandParam;
    bool readWrite = params.readWriteParam;
    int &cpid = params.cpidParam;
    bool isRoot = params.isRoot;
    constexpr uint8_t pipeRead = 0;
    constexpr uint8_t pipeWrite = 1;
    pid_t childPid;
    int fds[2];
    pipe(fds);
    WRITE_LOG(LOG_DEBUG, "Popen pipe fds[pipeRead]:%d fds[pipeWrite]:%d, mode %d",
        fds[pipeRead], fds[pipeWrite], isRoot);

    if ((childPid = fork()) == -1) {
        return reinterpret_cast<void *>(ERR_GENERIC);
    }
    if (childPid == 0) {
        WRITE_LOG(LOG_DEBUG, "Popen close pipe fds[pipeRead]:%d fds[pipeWrite]:%d",
            fds[pipeRead], fds[pipeWrite]);
        Base::DeInitProcess();
        // avoid cpu 100% when watch -n 2 ls command
        dup2(fds[pipeRead], STDIN_FILENO);
        if (readWrite) {
            dup2(fds[pipeWrite], STDOUT_FILENO);
            dup2(fds[pipeWrite], STDERR_FILENO);
        }
        close(fds[pipeRead]);
        close(fds[pipeWrite]);

        setsid();
        setpgid(childPid, childPid);
#if !defined(_WIN32) && !defined(HDC_HOST)
        SetSelinuxLabel(isRoot);
#endif
        string shellPath = Base::GetShellPath();
        execl(shellPath.c_str(), shellPath.c_str(), "-c", command.c_str(), NULL);
    } else {
        if (readWrite) {
            Base::CloseFd(fds[pipeWrite]);
            fcntl(fds[pipeRead], F_SETFD, FD_CLOEXEC);
        } else {
            Base::CloseFd(fds[pipeRead]);
            fcntl(fds[pipeWrite], F_SETFD, FD_CLOEXEC);
        }
    }
    cpid = childPid;
    if (readWrite) {
        return reinterpret_cast<void *>(fds[pipeRead]);
    } else {
        return reinterpret_cast<void *>(fds[pipeWrite]);
    }
#endif
}

bool AsyncCmd::ExecuteCommand(const string &command)
{
    string cmd = command;
    Base::Trim(cmd, "\"");
    if ((fd = ThreadFork(cmd, true, pid)) < 0) {
        return false;
    }
    WRITE_LOG(LOG_DEBUG, "ExecuteCommand cmd:%s fd:%d pid:%d", cmd.c_str(), fd, pid);
    childShell = new(std::nothrow) HdcFileDescriptor(loop, fd, this, ChildReadCallback, FinishShellProc);
    if (childShell == nullptr) {
        WRITE_LOG(LOG_FATAL, "ExecuteCommand new childShell failed");
        return false;
    }
    if (!childShell->StartWorkOnThread()) {
        return false;
    }
    ++refCount;
    return true;
}
}  // namespace Hdc
