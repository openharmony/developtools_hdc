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
#include "shell.h"
#include <sys/wait.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <parameters.h>
#include <string>
#include "fcntl.h"
#include "functional"
#include "new"
#include <pthread.h>
#include "unistd.h"
#include "base.h"
#include "file_descriptor.h"
#include "system_depend.h"
#if defined(SURPPORT_SELINUX)
#include "selinux/selinux.h"
#endif
#include "daemon_base.h"

namespace Hdc {
std::mutex HdcShell::mutexPty;

HdcShell::HdcShell(HTaskInfo hTaskInfo)
    : HdcTaskBase(hTaskInfo)
{
    childShell = nullptr;
    fdPTY = 0;
}

HdcShell::~HdcShell()
{
    WRITE_LOG(LOG_DEBUG, "~HdcShell channelId:%u", taskInfo->channelId);
};

bool HdcShell::ReadyForRelease()
{
    if (!HdcTaskBase::ReadyForRelease()) {
        WRITE_LOG(LOG_WARN, "not ready for release channelId:%u", taskInfo->channelId);
        return false;
    }
    if (!childReady) {
        WRITE_LOG(LOG_WARN, "childReady false channelId:%u", taskInfo->channelId);
        return true;
    }
    if (!childShell->ReadyForRelease()) {
        WRITE_LOG(LOG_WARN, "childShell not ready for release channelId:%u", taskInfo->channelId);
        return false;
    }
    delete childShell;
    childShell = nullptr;
    WRITE_LOG(LOG_DEBUG, "ReadyForRelease close fdPTY:%d", fdPTY);
    Base::CloseFd(fdPTY);
    return true;
}

void HdcShell::StopTask()
{
    singalStop = true;
    WRITE_LOG(LOG_DEBUG, "StopTask pidShell:%d childReady:%d", pidShell, childReady);
    if (!childReady) {
        return;
    }
    if (childShell) {
        childShell->StopWorkOnThread(false, nullptr);
    }

    if (pidShell > 1) {
        kill(pidShell, SIGKILL);
        int status;
        waitpid(pidShell, &status, 0);
        WRITE_LOG(LOG_DEBUG, "StopTask, kill pidshell:%d", pidShell);
    }
};

bool HdcShell::SpecialSignal(uint8_t ch)
{
    const uint8_t TXT_SIGNAL_ETX = 0x3;
    bool ret = true;
    switch (ch) {
        case TXT_SIGNAL_ETX: {  // Ctrl+C
            if (fdPTY <= 0) {
                break;
            }
            pid_t tpgid = tcgetpgrp(fdPTY);
            if (tpgid > 1) {
                kill(tpgid, SIGINT);
            }
            break;
        }
        default:
            ret = false;
            break;
    }
    return ret;
}

bool HdcShell::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    switch (command) {
        case CMD_SHELL_INIT: {  // initial
            if (!InitShell(payload, payloadSize)) {
                return false;
            }
            if (StartShell() < 0) {
                LogMsg(MSG_FAIL, "Shell initialize failed");
                return false;
            }
            break;
        }
        case CMD_SHELL_DATA:
            if (!childReady) {
                WRITE_LOG(LOG_DEBUG, "Shell not running");
                return false;
            }
            if (payloadSize == 1 && SpecialSignal(payload[0])) {
            } else {
                int ret = childShell->Write(payload, payloadSize);
                if (ret < 0) {
                    return false;
                }
            }
            break;
        default:
            break;
    }
    return true;
}

#if defined(HDC_SUPPORT_REPORT_COMMAND_EVENT)
static bool IsShellHistoryEnable()
{
    return OHOS::system::GetParameter("persist.hdc.shell_history.enable", "") == "true";
}

static std::string IntArr2HexStr(uint8_t arr[], int length)
{
    std::stringstream ss;
    const int byteHexStrLen = 2;
    for (int i = 0; i < length; i++) {
        ss << std::hex << std::setw(byteHexStrLen) << std::setfill('0')
            << static_cast<int>(arr[i]);
    }
    std::string result = ss.str();
    return result;
}

static std::string GetHistoryFileName(uint32_t sessionId)
{
    std::string sessionIdStr = std::to_string(sessionId);
    unsigned char sha512Hash[SHA512_DIGEST_LENGTH];
    memset_s(sha512Hash, SHA512_DIGEST_LENGTH, 0, SHA512_DIGEST_LENGTH);
    if (SHA512(reinterpret_cast<const unsigned char *>(sessionIdStr.c_str()), sessionIdStr.size(), sha512Hash)
        == nullptr) {
        WRITE_LOG(LOG_WARN, "GetHistoryFileName calc sha512 failed, sid:%s",
            Hdc::MaskSessionIdToString(sessionId).c_str());
        return "";
    }

    // Take the first 16 characters of the sha512 hexadecimal string as the filename
    static constexpr int historyFileNameLength = 16;
    std::string sha512hexStr = IntArr2HexStr(sha512Hash, SHA512_DIGEST_LENGTH);
    if (sha512hexStr.length() < historyFileNameLength) {
        WRITE_LOG(LOG_WARN, "GetHistoryFileName sha512 hex string length is not correct, len:%zu, sid:%s",
            sha512hexStr.length(), Hdc::MaskSessionIdToString(sessionId).c_str());
        return "";
    }
    const std::string historyFilePath = "/data/local/tmp/ecd/";
    std::string historyFileName = historyFilePath + sha512hexStr.substr(0, historyFileNameLength);
    WRITE_LOG(LOG_INFO, "GetHistoryFileName finished, sid:%s", Hdc::MaskSessionIdToString(sessionId).c_str());
    return historyFileName;
}

static void ConfigHistoryFile(const std::string &historyFileName)
{
    const std::string historyFile = "HISTFILE";
    const std::string historyFileSize = "HISTFILESIZE";
    const std::string historyFileSizeValue = "20000";

    if (historyFileName.empty()) {
        WRITE_LOG(LOG_WARN, "history file name is empty");
        return;
    }
    if (setenv(historyFile.c_str(), historyFileName.c_str(), 1) < 0) {
        WRITE_LOG(LOG_WARN, "setenv history file failed, errno:%d", errno);
        return;
    }
    if (setenv(historyFileSize.c_str(), historyFileSizeValue.c_str(), 1) < 0) {
        WRITE_LOG(LOG_WARN, "setenv history file size failed, errno:%d", errno);
        return;
    }
    WRITE_LOG(LOG_INFO, "ConfighistoryFile finished");
}
#endif

int HdcShell::ChildForkDo(int pts, const ShellParams &params)
{
    dup2(pts, STDIN_FILENO);
    dup2(pts, STDOUT_FILENO);
    dup2(pts, STDERR_FILENO);
    close(pts);
    string text = "/proc/self/oom_score_adj";
    int fd = 0;
    if ((fd = open(text.c_str(), O_WRONLY)) >= 0) {
        write(fd, "0", 1);
        close(fd);
    }

#if defined(HDC_SUPPORT_REPORT_COMMAND_EVENT)
    if (params.isShellHistoryEnable) {
        ConfigHistoryFile(params.historyFileName);
    }
#endif
    char *env = nullptr;
    const char *workDir = params.workDirParam.empty() ? nullptr : params.workDirParam.c_str();
    if (workDir != nullptr && strlen(workDir) > 0 && chdir(workDir) == 0) {
        WRITE_LOG(LOG_DEBUG, "chdir to %s", workDir);
    } else if (((env = getenv("HOME")) && chdir(env) < 0) || chdir("/")) {
        if (workDir != nullptr && strlen(workDir) > 0) {
            WRITE_LOG(LOG_FATAL, "chdir bundlePath %s failed, %s", workDir, strerror(errno));
        }
    }
    int ret = execl(params.cmdParam, params.cmdParam, params.arg0Param, params.arg1Param, nullptr);
    if (ret < 0) {
        WRITE_LOG(LOG_DEBUG, "execl failed, %s", strerror(errno));
        _exit(0);
    }
    return 0;
}

static void SetSelinuxLabel()
{
#if defined(SURPPORT_SELINUX)
    char *con = nullptr;
    if (getcon(&con) != 0) {
        return;
    }
    if ((con != nullptr) && (strcmp(con, "u:r:hdcd:s0") != 0) && (strcmp(con, "u:r:updater:s0") != 0)) {
        freecon(con);
        return;
    }
    string debugMode = "";
    SystemDepend::GetDevItem("const.debuggable", debugMode);
    if (debugMode != "1") {
        setcon("u:r:sh:s0");
    } else {
        string rootMode = "";
        string flashdMode = "";
        SystemDepend::GetDevItem("persist.hdc.root", rootMode);
        SystemDepend::GetDevItem("updater.flashd.configfs", flashdMode);
        if ((debugMode == "1" && rootMode == "1") || (debugMode == "1" && flashdMode == "1")) {
            setcon("u:r:su:s0");
        } else {
            setcon("u:r:sh:s0");
        }
    }
    freecon(con);
#endif
}

int HdcShell::ThreadFork(const char *cmd, const char *arg0, const char *arg1)
{
    const char *workDir = optionPath.empty() ? nullptr : optionPath.c_str();
    auto params = new ShellParams{
        .cmdParam = cmd,
        .arg0Param = arg0,
        .arg1Param = arg1,
        .ptmParam = ptm,
        .devParam = devname,
        .workDirParam = workDir == nullptr ? "" : workDir
    };
    if (!params) {
        WRITE_LOG(LOG_DEBUG, "shell params nullptr ptm:%d", ptm);
        return -1;
    }
#if defined(HDC_SUPPORT_REPORT_COMMAND_EVENT)
    params->isShellHistoryEnable = IsShellHistoryEnable();
    if (params->isShellHistoryEnable) {
        params->historyFileName = GetHistoryFileName(taskInfo->sessionId);
    }
#endif
    pthread_t threadId;
    void *shellRes;
    int ret = pthread_create(&threadId, nullptr, reinterpret_cast<void *(*)(void *)>(ShellFork), params);
    if (ret != 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        strerror_r(errno, buf, bufSize);
        WRITE_LOG(LOG_DEBUG, "fork Thread create failed:%s", buf);
        delete params;
        return ERR_GENERIC;
    }
    pthread_join(threadId, &shellRes);
    delete params;
    return static_cast<int>(reinterpret_cast<size_t>(shellRes));
}

void *HdcShell::ShellFork(void *arg)
{
    int ret = pthread_setname_np(pthread_self(), "hdcd_shellfork");
    if (ret != 0) {
        WRITE_LOG(LOG_DEBUG, "set Thread name failed.");
    }
    ShellParams params = *reinterpret_cast<ShellParams *>(arg);
    int ptmParam = params.ptmParam;
    char *devParam = params.devParam;
    pid_t pid = 0;
    pid = fork();
    if (pid < 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        strerror_r(errno, buf, bufSize);
        WRITE_LOG(LOG_FATAL, "Fork shell failed:%s", buf);
        return reinterpret_cast<void *>(ERR_GENERIC);
    }
    if (pid == 0) {
        WRITE_LOG(LOG_DEBUG, "ShellFork close ptmParam:%d", ptmParam);
        Base::DeInitProcess();
        HdcShell::mutexPty.unlock();
        setsid();
        SetSelinuxLabel();
        close(ptmParam);
        int pts = 0;
        if ((pts = open(devParam, O_RDWR | O_CLOEXEC)) < 0) {
            return reinterpret_cast<void *>(-1);
        }
        ChildForkDo(pts, params);
        // proc finish
    } else {
        return reinterpret_cast<void *>(pid);
    }
    return reinterpret_cast<void *>(0);
}

int HdcShell::CreateSubProcessPTY(const char *cmd, const char *arg0, const char *arg1, pid_t *pid)
{
    ptm = open(devPTMX.c_str(), O_RDWR | O_CLOEXEC);
    if (ptm < 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        strerror_r(errno, buf, bufSize);
        WRITE_LOG(LOG_DEBUG, "Cannot open ptmx, error:%s", buf);
        return ERR_FILE_OPEN;
    }
    if (grantpt(ptm) || unlockpt(ptm)) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        strerror_r(errno, buf, bufSize);
        WRITE_LOG(LOG_DEBUG, "Cannot open2 ptmx, error:%s", buf);
        Base::CloseFd(ptm);
        return ERR_API_FAIL;
    }
    if (ptsname_r(ptm, devname, sizeof(devname)) != 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        strerror_r(errno, buf, bufSize);
        WRITE_LOG(LOG_DEBUG, "Trouble with  ptmx, error:%s", buf);
        Base::CloseFd(ptm);
        return ERR_API_FAIL;
    }
    int rc = ThreadFork(cmd, arg0, arg1);
    if (rc < 0) {
        WRITE_LOG(LOG_WARN, "get pid failed rc:%d", rc);
    } else {
        *pid = rc;
    }
    return ptm;
}

bool HdcShell::FinishShellProc(const void *context, const bool /* result */, const string /* exitMsg */)
{
    WRITE_LOG(LOG_DEBUG, "FinishShellProc finish");
    HdcShell *thisClass = reinterpret_cast<HdcShell *>(const_cast<void *>(context));
    thisClass->TaskFinish();
    --thisClass->refCount;
    return true;
};

bool HdcShell::ChildReadCallback(const void *context, uint8_t *buf, const int size)
{
    HdcShell *thisClass = reinterpret_cast<HdcShell *>(const_cast<void *>(context));
    return thisClass->SendToAnother(CMD_KERNEL_ECHO_RAW, reinterpret_cast<uint8_t *>(buf), size);
};

bool HdcShell::InitShell(uint8_t *payload, const int payloadSize)
{
#ifndef UPDATER_MODE
    TlvBuf tlvBuf(const_cast<uint8_t *>(payload), payloadSize, Base::REGISTERD_TAG_SET);
    std::string bundleName = "";
    if (payloadSize > 0 && payload != nullptr && tlvBuf.FindTlv(TAG_SHELL_BUNDLE, bundleName)) {
        std::string mountPath;
        if (!HdcDaemonBase::CheckBundlePath(bundleName, mountPath)) {
            LogMsg(MSG_FAIL, "[E003001] Invalid bundle name: %s", bundleName.c_str());
            return false;
        }
        optionPath = mountPath;
    } else {
        optionPath.clear();
    }
#else
    optionPath.clear();
#endif
    return true;
}

int HdcShell::StartShell()
{
    int ret = 0;
    HdcShell::mutexPty.lock();
    do {
        if ((fdPTY = CreateSubProcessPTY(Base::GetShellPath().c_str(), "-", 0, &pidShell)) < 0) {
            ret = ERR_PROCESS_SUB_FAIL;
            break;
        }
        childShell = new(std::nothrow) HdcFileDescriptor(loopTask, fdPTY, this, ChildReadCallback,
                                                         FinishShellProc, true);
        if (childShell == nullptr) {
            WRITE_LOG(LOG_FATAL, "StartShell new childShell failed");
            ret = ERR_GENERIC;
            break;
        }
        if (!childShell->StartWorkOnThread()) {
            WRITE_LOG(LOG_FATAL, "StartShell childShell->StartWorkOnThread false");
            ret = ERR_API_FAIL;
            break;
        }
        childReady = true;
        ++refCount;
    } while (false);
    WRITE_LOG(LOG_DEBUG, "StartShell pid:%d channelId:%u ret:%d", pidShell, taskInfo->channelId, ret);
    if (ret != RET_SUCCESS) {
        if (pidShell > 0) {
            kill(pidShell, SIGKILL);
        }
        // fdPTY close by ~clase
    }
    HdcShell::mutexPty.unlock();
    return ret;
}
}  // namespace Hdc
