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
    if (Base::CloseFd(fd) != 0) {
        WRITE_LOG(LOG_INFO, "AsyncCmd Release, close fd:%d", fd);
    }
    if (childShell != nullptr) {
        delete childShell;
        childShell = nullptr;
    }
};

bool AsyncCmd::ReadyForRelease()
{
    if (childShell != nullptr && !childShell->ReadyForRelease()) {
        WRITE_LOG(LOG_WARN, "childShell not ready for release pid:%d", pid);
        return false;
    }
    uint32_t count = refCount.load();
    if (count != 0) {
        WRITE_LOG(LOG_WARN, "refCount:%u not ready for release", count);
        return false;
    }
    if (childShell != nullptr) {
        delete childShell;
        childShell = nullptr;
    }
    return true;
}

void AsyncCmd::DoRelease()
{
    if (childShell != nullptr) {
        childShell->StopWorkOnThread(false, nullptr);
    }
    int rc = 0;
    if (pid > 0) {
        rc = uv_kill(pid, SIGTERM);
    }
    WRITE_LOG(LOG_INFO, "DoRelease rc:%d pid:%d", rc, pid);
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
    WRITE_LOG(LOG_DEBUG, "FinishShellProc finish pipeRead fd:%d pid:%d count:%u",
              thisClass->fd, thisClass->pid, thisClass->refCount.load());
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
        WRITE_LOG(LOG_WARN, "GetDevItem false key:%s", key);
        return false;
    }
#else
    string sFailString = Base::StringFormat("Get parameter \"%s\" fail", key);
    string stringBuf = "param get " + string(key);
    Base::RunPipeComand(stringBuf.c_str(), tmpStringBuf, BUF_SIZE_MEDIUM - 1, true);
    if (!strcmp(sFailString.c_str(), tmpStringBuf)) {
        WRITE_LOG(LOG_WARN, "GetDevItem false tmpStringBuf:%s", tmpStringBuf);
        ret = false;
        (void)memset_s(tmpStringBuf, sizeof(tmpStringBuf), 0, sizeof(tmpStringBuf));
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
        WRITE_LOG(LOG_WARN, "SetSelinuxLabel isRoot:%d", isRoot);
        return;
    }
    if ((con != nullptr) && (strcmp(con, "u:r:hdcd:s0") != 0) && (strcmp(con, "u:r:updater:s0") != 0)) {
        WRITE_LOG(LOG_WARN, "SetSelinuxLabel con:%s isRoot:%d", con, isRoot);
        freecon(con);
        return;
    }
    if (isRoot) {
        setcon("u:r:su:s0");
    } else {
        setcon("u:r:sh:s0");
    }
    freecon(con);
#endif
}
#endif

int AsyncCmd::ThreadFork(const string &command, const string &optionPath, bool readWrite, int &cpid)
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
    auto params = new AsyncParams(command, readWrite, cpid, isRoot, optionPath);
    if (!params) {
        return -1;
    }
    pthread_t threadId;
    void *popenRes;
    int ret = pthread_create(&threadId, nullptr, reinterpret_cast<void *(*)(void *)>(Popen), params);
    if (ret != 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
#ifdef _WIN32
        strerror_s(buf, bufSize, errno);
#else
        strerror_r(errno, buf, bufSize);
#endif
        WRITE_LOG(LOG_DEBUG, "fork Thread create failed:%s", buf);
        delete params;
        return ERR_GENERIC;
    }
    pthread_join(threadId, &popenRes);
    delete params;
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
    auto param = reinterpret_cast<AsyncParams *>(arg);
    if (param == nullptr) {
        WRITE_LOG(LOG_FATAL, "get param is nullptr.");
        return reinterpret_cast<void *>(ERR_PARAM_NULLPTR);
    }
    AsyncParams params = *param;
    string command = params.commandParam;
    bool readWrite = params.readWriteParam;
    int &cpid = params.cpidParam;
    bool isRoot = params.isRoot;
    constexpr uint8_t pipeRead = 0;
    constexpr uint8_t pipeWrite = 1;
    pid_t childPid;
    int fds[2];
    if (pipe(fds) != 0) {
        WRITE_LOG(LOG_FATAL, "Popen pipe failed errno:%d", errno);
        return reinterpret_cast<void *>(ERR_GENERIC);
    }
    WRITE_LOG(LOG_DEBUG, "Popen pipe fds[pipeRead]:%d fds[pipeWrite]:%d, mode %d",
        fds[pipeRead], fds[pipeWrite], isRoot);

    if ((childPid = fork()) == -1) {
        WRITE_LOG(LOG_FATAL, "Popen fork failed errno:%d", errno);
        return reinterpret_cast<void *>(ERR_GENERIC);
    }
    if (childPid == 0) {
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
        int execlRet = 0;
        if (!params.optionPath.empty() && chdir(params.optionPath.c_str()) != 0) {
            string cmdEcho = "echo \"[E003006] Internal error: AsyncCmd chdir failed:" + params.optionPath + "\"";
            execlRet = execl(shellPath.c_str(), shellPath.c_str(), "-c", cmdEcho.c_str(), NULL);
        } else {
            execlRet = execl(shellPath.c_str(), shellPath.c_str(), "-c", command.c_str(), NULL);
        }
        if (execlRet < 0) {
            WRITE_LOG(LOG_FATAL, "start shell failed %d: %s", execlRet, strerror(errno));
            _exit(0);
        }
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

bool AsyncCmd::ExecuteCommand(const string &command, string executePath)
{
    string cmd = command;
    cmd = Base::ShellCmdTrim(cmd);
    if ((fd = ThreadFork(cmd, executePath, true, pid)) < 0) {
        WRITE_LOG(LOG_FATAL, "ExecuteCommand failed cmd:%s fd:%d", Hdc::MaskString(cmd).c_str(), fd);
        return false;
    }
    WRITE_LOG(LOG_DEBUG, "ExecuteCommand cmd:%s fd:%d pid:%d", Hdc::MaskString(cmd).c_str(), fd, pid);
    ++refCount;
    childShell = new(std::nothrow) HdcFileDescriptor(loop, fd, this, ChildReadCallback, FinishShellProc, false);
    if (childShell == nullptr) {
        WRITE_LOG(LOG_FATAL, "ExecuteCommand new childShell failed");
        --refCount;
        return false;
    }
    if (!childShell->StartWorkOnThread()) {
        WRITE_LOG(LOG_FATAL, "ExecuteCommand StartWorkOnThread failed");
        return false;
    }
    return true;
}
}  // namespace Hdc
