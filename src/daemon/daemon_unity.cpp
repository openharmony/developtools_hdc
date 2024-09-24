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
#include <cstdlib>
#include <sys/mount.h>
#include <sys/wait.h>

#include "daemon_unity.h"

namespace Hdc {
HdcDaemonUnity::HdcDaemonUnity(HTaskInfo hTaskInfo)
    : HdcTaskBase(hTaskInfo)
{
    currentDataCommand = CMD_KERNEL_ECHO_RAW;  // Default output to shelldata
}

HdcDaemonUnity::~HdcDaemonUnity()
{
    WRITE_LOG(LOG_DEBUG, "~HdcDaemonUnity channelId:%u", taskInfo->channelId);
}

void HdcDaemonUnity::StopTask()
{
    // Remove jpid tracker when stopping task
    RemoveJdwpTracker();
    asyncCommand.DoRelease();
}

bool HdcDaemonUnity::ReadyForRelease()
{
    if (!HdcTaskBase::ReadyForRelease() || !asyncCommand.ReadyForRelease()) {
        WRITE_LOG(LOG_DEBUG, "not ready for release channelId:%u", taskInfo->channelId);
        return false;
    }
    return true;
}

bool HdcDaemonUnity::AsyncCmdOut(bool finish, int64_t exitStatus, const string result)
{
#ifdef UNIT_TEST
    Base::WriteBinFile((UT_TMP_PATH + "/execute.result").c_str(), (uint8_t *)result.c_str(), result.size(),
                       countUt++ == 0);
#endif
    bool ret = false;
    bool wantFinish = false;
    do {
        if (finish) {
            wantFinish = true;
            ret = true;
            --refCount;
            break;
        }
        if (!SendToAnother(currentDataCommand, reinterpret_cast<uint8_t *>(const_cast<char *>(result.c_str())),
            result.size())) {
            break;
        }
        ret = true;
    } while (false);
    if (wantFinish) {
        TaskFinish();
    }
    return ret;
}

int HdcDaemonUnity::ExecuteShell(const char *shellCommand)
{
    do {
        AsyncCmd::CmdResultCallback funcResultOutput;
        funcResultOutput = [this](bool finish, int64_t exitStatus, const string result) -> bool {
            return this->AsyncCmdOut(finish, exitStatus, result);
        };
        if (!asyncCommand.Initial(loopTask, funcResultOutput)) {
            break;
        }
        asyncCommand.ExecuteCommand(shellCommand);
        ++refCount;
        return RET_SUCCESS;
    } while (false);

    TaskFinish();
    WRITE_LOG(LOG_DEBUG, "Shell failed finish");
    return -1;
}

bool HdcDaemonUnity::FindMountDeviceByPath(const char *toQuery, char *dev)
{
    int ret = false;
    int len = BUF_SIZE_DEFAULT2;
    char buf[BUF_SIZE_DEFAULT2];

    FILE *fp = fopen("/proc/mounts", "r");
    if (fp == nullptr) {
        WRITE_LOG(LOG_FATAL, "fopen /proc/mounts error:%d", errno);
        return false;
    }

    while (fgets(buf, len, fp) != nullptr) {
        char dir[BUF_SIZE_SMALL] = "";
        int freq;
        int passnno;
        int res = 0;
        // clang-format off
        res = sscanf_s(buf, "%255s %255s %*s %*s %d %d\n", dev, BUF_SIZE_SMALL - 1,
                       dir, BUF_SIZE_SMALL - 1, &freq, &passnno);
        // clang-format on
        dev[BUF_SIZE_SMALL - 1] = '\0';
        dir[BUF_SIZE_SMALL - 1] = '\0';
        if (res == 4 && (strcmp(toQuery, dir) == 0)) {  // 4 : The correct number of parameters
            WRITE_LOG(LOG_DEBUG, "FindMountDeviceByPath dev:%s dir:%s", dev, dir);
            ret = true;
            break;
        }
    }
    int rc = fclose(fp);
    if (rc != 0) {
        WRITE_LOG(LOG_WARN, "fclose rc:%d error:%d", rc, errno);
    }
    if (!ret) {
        WRITE_LOG(LOG_FATAL, "FindMountDeviceByPath not found %s", toQuery);
    }
    return ret;
}

bool HdcDaemonUnity::RemountPartition(const char *dir)
{
    int fd;
    int off = 0;
    char dev[BUF_SIZE_SMALL] = "";

    if (!FindMountDeviceByPath(dir, dev) || strlen(dev) < 4) {  // 4 : file count
        WRITE_LOG(LOG_FATAL, "FindMountDeviceByPath failed %s", dir);
        return false;
    }

    if ((fd = open(dev, O_RDONLY | O_CLOEXEC)) < 0) {
        WRITE_LOG(LOG_FATAL, "Open device:%s failed，error:%d", dev, errno);
        return false;
    }
    ioctl(fd, BLKROSET, &off);
    Base::CloseFd(fd);

    if (mount(dev, dir, "none", MS_REMOUNT, nullptr) < 0) {
        WRITE_LOG(LOG_FATAL, "Mount device failed dev:%s dir:%s error:%d", dev, dir, errno);
        return false;
    }
    return true;
}

bool HdcDaemonUnity::CallRemount()
{
    int pipefd[2];
    pid_t pid;
    int exitStatus;

    if (pipe(pipefd) == -1) {
        WRITE_LOG(LOG_FATAL, "Failed to create pipe: %s", strerror(errno));
        return false;
    }
    pid = fork();
    if (pid < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to fork: %s", strerror(errno));
        return false;
    }
    if (pid == 0) {
        close(pipefd[0]);
        signal(SIGCHLD, SIG_DFL);
        exitStatus = system("remount");
        write(pipefd[1], &exitStatus, sizeof(int));
        close(pipefd[1]);
        _exit(0);
    } else {
        close(pipefd[1]);
        read(pipefd[0], &exitStatus, sizeof(int));
        close(pipefd[0]);
        waitpid(pid, nullptr, 0);
        if (exitStatus == -1) {
            WRITE_LOG(LOG_FATAL, "Failed to execute /bin/remount: %s", strerror(errno));
            return false;
        } else if (WIFEXITED(exitStatus) && WEXITSTATUS(exitStatus) != 0) {
            WRITE_LOG(LOG_FATAL, "Remount failed with exit code: %d", WEXITSTATUS(exitStatus));
            return false;
        }
    }

    return true;
}

bool HdcDaemonUnity::RemountDevice()
{
    if (getuid() != 0) {
        LogMsg(MSG_FAIL, "Operate need running as root");
        return false;
    }
    struct stat info;
    if (!lstat("/vendor", &info) && (info.st_mode & S_IFMT) == S_IFDIR) {
        if (!RemountPartition("/vendor")) {
            WRITE_LOG(LOG_FATAL, "Mount failed /vendor (via mount)");
        }
    }
    if (!lstat("/system", &info) && (info.st_mode & S_IFMT) == S_IFDIR) {
        if (!RemountPartition("/")) {
            WRITE_LOG(LOG_FATAL, "Mount failed /system (via mount)");
        }
    }

    if (CallRemount()) {
        LogMsg(MSG_OK, "Mount finish");
        return true;
    } else {
        LogMsg(MSG_FAIL, "Mount failed");
        return false;
    }
}

bool HdcDaemonUnity::RebootDevice(const string &cmd)
{
    sync();
    return SystemDepend::RebootDevice(cmd);
}

bool HdcDaemonUnity::SetDeviceRunMode(const char *cmd)
{
    WRITE_LOG(LOG_INFO, "Set run mode:%s", cmd);
    string tmp(cmd);
    char *ptr = tmp.data();
    char *token = nullptr;

#ifdef HDC_EMULATOR
    LogMsg(MSG_FAIL, "[E001300]Not support tmode for Emulator");
    return true;
#endif

    // hdc tmode usb, do nothing
    if (strcmp(CMDSTR_TMODE_USB.c_str(), cmd) == 0) {
        LogMsg(MSG_FAIL, "[E001000]For USB debugging, please set it on the device's Settings UI");
        return true;
    }
    // not usb and not tcp
    if (strncmp("port", cmd, strlen("port")) != 0) {
        LogMsg(MSG_FAIL, "[E001001]Unknown command");
        return false;
    }

    // bypass port
    token = strtok_r(ptr, " ", &ptr);
    // get next token
    token = strtok_r(ptr, " ", &ptr);
    // hdc tmode port
    if (token == nullptr) {
        LogMsg(MSG_OK, "Set device run mode successful.");
        SystemDepend::SetDevItem("persist.hdc.mode", "tcp");
        SystemDepend::SetDevItem("persist.hdc.mode.tcp", "enable");
    } else {
        /*
        * hdc tmode port xxxxxx
        * hdc tmode port close
        */
        if (strcmp(token, "close") == 0) {
            SystemDepend::SetDevItem("persist.hdc.port", "0");
            SystemDepend::SetDevItem("persist.hdc.mode.tcp", "disable");
        } else {
            string tmp(token);
            if (tmp.find_first_not_of("0123456789") != string::npos) {
                LogMsg(MSG_FAIL, "[E001100]Invalid port");
                return false;
            }
            LogMsg(MSG_OK, "Set device run mode successful.");
            SystemDepend::SetDevItem("persist.hdc.mode", "tcp");
            SystemDepend::SetDevItem("persist.hdc.port", token);
            SystemDepend::SetDevItem("persist.hdc.mode.tcp", "enable");
        }
    }
    return true;
}

inline bool HdcDaemonUnity::GetHiLog(const char *cmd)
{
    string cmdDo = "hilog";
    if (cmd && !strcmp(const_cast<char *>(cmd), "h")) {
        cmdDo += " -h";
    }
    ExecuteShell(cmdDo.c_str());
    return true;
}

inline bool HdcDaemonUnity::ListJdwpProcess(void *daemonIn)
{
    HdcDaemon *daemon = (HdcDaemon *)daemonIn;
    string result = ((HdcJdwp *)daemon->clsJdwp)->GetProcessList();
    if (!result.size()) {
        result = EMPTY_ECHO;
    } else {
        result.erase(result.end() - 1);  // remove tail \n
    }
    LogMsg(MSG_OK, result.c_str());
    return true;
}

inline bool HdcDaemonUnity::TrackJdwpProcess(void *daemonIn, const string& param)
{
    HdcDaemon *daemon = static_cast<HdcDaemon *>(daemonIn);
    taskInfo->debugRelease = 1;
    if (param == "p") {
        taskInfo->debugRelease = 0;
    } else if (param == "a") {
        // allApp with display debug or release
        constexpr uint8_t allAppWithDr = 3;
        taskInfo->debugRelease = allAppWithDr;
    }
    if (!((static_cast<HdcJdwp *>(daemon->clsJdwp))->CreateJdwpTracker(taskInfo))) {
        string result = MESSAGE_FAIL;
        LogMsg(MSG_OK, result.c_str());
        return false;
    }
    return true;
}

inline void HdcDaemonUnity::RemoveJdwpTracker()
{
    HdcDaemon *daemon = static_cast<HdcDaemon *>(taskInfo->ownerSessionClass);
    (static_cast<HdcJdwp *>(daemon->clsJdwp))->RemoveJdwpTracker(taskInfo);
}

bool HdcDaemonUnity::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    bool ret = true;
    HdcDaemon *daemon = (HdcDaemon *)taskInfo->ownerSessionClass;
    // Both are not executed, do not need to be detected 'childReady'
    string strPayload = string(reinterpret_cast<char *>(payload), payloadSize);
#ifdef HDC_DEBUG
    WRITE_LOG(LOG_DEBUG, "CommandDispatch command:%d", command);
#endif // HDC_DEBUG
    switch (command) {
        case CMD_UNITY_EXECUTE: {
            ExecuteShell(const_cast<char *>(strPayload.c_str()));
            break;
        }
        case CMD_UNITY_REMOUNT: {
            ret = false;
            RemountDevice();
            break;
        }
        case CMD_UNITY_REBOOT: {
            ret = false;
            RebootDevice(strPayload);
            break;
        }
        case CMD_UNITY_RUNMODE: {
            ret = false;
            SetDeviceRunMode(strPayload.c_str());
            break;
        }
        case CMD_UNITY_HILOG: {
            GetHiLog(strPayload.c_str());
            break;
        }
        case CMD_UNITY_ROOTRUN: {
            ret = false;
            string debugMode;
            // hdcd restart in old pid
            bool restart = true;
            SystemDepend::GetDevItem("const.debuggable", debugMode);
            if (debugMode == "1") {
                if (payloadSize != 0 && !strcmp(strPayload.c_str(), "r")) {
                    SystemDepend::SetDevItem("persist.hdc.root", "0");
                } else {
                    // hdcd restart in new pid
                    restart = false;
                    SystemDepend::SetDevItem("persist.hdc.root", "1");
                }
            } else {
                LogMsg(MSG_FAIL, "Cannot set root run mode in undebuggable version.");
                return false;
            }
            daemon->PostStopInstanceMessage(restart);
            break;
        }
        case CMD_UNITY_BUGREPORT_INIT: {
            currentDataCommand = CMD_UNITY_BUGREPORT_DATA;
            ExecuteShell("hidumper");
            break;
        }
        case CMD_JDWP_LIST: {
            ret = false;
            ListJdwpProcess(daemon);
            break;
        }
        case CMD_JDWP_TRACK: {
            if (!TrackJdwpProcess(daemon, strPayload)) {
                ret = false;
            }
            break;
        }
        default:
            break;
    }
#ifdef HDC_DEBUG
    WRITE_LOG(LOG_DEBUG, "CommandDispatch command:%d finish.", command);
#endif // HDC_LOCAL_DEBUG
    return ret;
};
}  // namespace Hdc
