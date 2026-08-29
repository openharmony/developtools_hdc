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
#ifndef _WIN32
#include <cerrno>
#include <poll.h>
#endif

#include "base.h"
#ifndef TEST_HASH
#include "hdc_hash_gen.h"
#endif
#include "host_updater.h"
#include "server.h"
#ifdef __OHOS__
#include <sys/un.h>
#include "system_depend.h"
#endif
#include "file.h"
#include "subserver/subserver_manager.h"
#ifdef HDC_SUPPORT_REPORT_COMMAND_EVENT
#include "command_event_report.h"
#endif

std::map<std::string, std::string> g_lists;
bool g_show = true;
#ifdef __OHOS__
static const std::string SYS_PARAM_ENTERPRISE_HDC_DISABLE = "persist.edm.hdc_remote_disable";
static const int ENTERPRISE_HDC_DISABLE_ERR = -11;
#endif

namespace Hdc {
bool g_terminalStateChange = false;
static constexpr int32_t WAIT_FOR_SPAWN_MAX_TIMES = 10;
#ifndef _WIN32
static constexpr size_t STDIN_POLL_INTERVAL_BYTES = 40 * 1024;
static constexpr int STDIN_POLL_TIMEOUT_MS = 1;

static bool WriteStdoutAll(const char *data, size_t size)
{
    size_t offset = 0;
    while (offset < size) {
        errno = 0;
        size_t written = fwrite(data + offset, sizeof(char), size - offset, stdout);
        offset += written;
        if (written > 0) {
            if (ferror(stdout)) {
                clearerr(stdout);
            }
            continue;
        }
        int error = errno;
        clearerr(stdout);
        if (error == EINTR) {
            continue;
        }
        WRITE_LOG(LOG_WARN, "Write stdout failed, expected:%zu actual:%zu error:%d", size, offset, error);
        return false;
    }
    while (fflush(stdout) == EOF) {
        int error = errno;
        clearerr(stdout);
        if (error != EINTR) {
            WRITE_LOG(LOG_WARN, "Flush stdout failed, error:%d", error);
            return false;
        }
    }
    return true;
}

static void SetStdoutBlocking(uv_tty_t *stdoutTty)
{
    int rc = uv_stream_set_blocking(reinterpret_cast<uv_stream_t *>(stdoutTty), 1);
    if (rc < 0) {
        WRITE_LOG(LOG_WARN, "Set stdout blocking failed:%s", uv_strerror(rc));
    }
}
#endif

static bool FindCommandInject(const std::string& input)
{
    static const std::string injectChars = "|;&$<>`\\!\n";
    return input.find_first_of(injectChars) != std::string::npos;
}

HdcClient::HdcClient(const bool serverOrClient, const string &addrString, uv_loop_t *loopMainIn, bool checkVersion)
    : HdcChannelBase(serverOrClient, addrString, loopMainIn)
{
#ifdef __OHOS__
    serverAddress = addrString;
    channel = new(std::nothrow) HdcChannel();
    if (channel != nullptr) {
        channel->isUds = (serverAddress.empty() || serverAddress == UDS_STR);
    }
#endif
    if (MallocChannel(&channel) == 0) {  // free by logic
        WRITE_LOG(LOG_DEBUG, "init channel failed");
        if (channel != nullptr) {
            delete channel;
            channel = nullptr;
        }
    }
    debugRetryCount = 0;
#ifndef _WIN32
    (void)memset_s(&terminalState, sizeof(termios), 0, sizeof(termios));
#endif
    isCheckVersionCmd = checkVersion;
}

HdcClient::~HdcClient()
{
#ifndef _WIN32
    if (g_terminalStateChange) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminalState);
    }
#endif
    Base::TryCloseLoop(loopMain, "ExecuteCommand finish");
}

void HdcClient::NotifyInstanceChannelFree(HChannel hChannel)
{
#ifndef _WIN32
    stdinBytesSincePoll = 0;
#endif
    if (bShellInteractive) {
        WRITE_LOG(LOG_DEBUG, "Restore tty");
        ModifyTty(false, &hChannel->stdinTty);
    }
}

uint32_t HdcClient::GetLastPID()
{
    char bufPath[BUF_SIZE_MEDIUM] = "";
    char pidBuf[BUF_SIZE_TINY] = "";
    // get running pid to kill it
    size_t size = BUF_SIZE_MEDIUM;
#ifdef HOST_OHOS
    if (uv_os_homedir(bufPath, &size) < 0) {
        WRITE_LOG(LOG_FATAL, "Homepath failed");
        return 0;
    }
#else
    if (uv_os_tmpdir(bufPath, &size) < 0) {
        WRITE_LOG(LOG_FATAL, "Tmppath failed");
        return 0;
    }
#endif
    string path = Base::StringFormat("%s%c.%s.pid", bufPath, Base::GetPathSep(), SERVER_NAME.c_str());
    if (Base::ReadBinFile(path.c_str(), reinterpret_cast<void**>(&pidBuf), BUF_SIZE_TINY) <= 0) {
        WRITE_LOG(LOG_FATAL, "Read pid file failed");
        return 0;
    }

    pidBuf[BUF_SIZE_TINY - 1] = '\0';

    int pid = 0;
    if (!Base::StringToInt(pidBuf, pid)) {
        WRITE_LOG(LOG_FATAL, "Convert pid string failed");
        return 0;
    }
    return pid;
}

bool HdcClient::StartServer(const string &cmd)
{
    int serverStatus = Base::ProgramMutex(true);
    if (serverStatus < 0) {
        WRITE_LOG(LOG_DEBUG, "get server status failed, serverStatus:%d", serverStatus);
        return false;
    }

    // server is not running
    if (serverStatus == 0) {
        HdcServer::PullupServer(channelHostPort.c_str());
        return true;
    }

    // server is running
    if (cmd.find(" -r") == std::string::npos) {
        return true;
    }

    // restart server
    uint32_t pid = GetLastPID();
    if (pid == 0) {
        Base::PrintMessage(TERMINAL_HDC_PROCESS_FAILED.c_str());
        return false;
    }
    int rc = uv_kill(pid, SIGKILL);
    WRITE_LOG(LOG_DEBUG, "uv_kill rc:%d", rc);
    HdcServer::PullupServer(channelHostPort.c_str());
    return true;
}

bool HdcClient::ChannelCtrlServer(bool isRestart, const string &connectKey)
{
    // new version build channle to send Ctrl command to server
    int serverStatus = Base::ProgramMutex(true);
    if (serverStatus < 0) {
        WRITE_LOG(LOG_DEBUG, "get server status failed, serverStatus:%d", serverStatus);
        return false;
    }
    int rc = Initial(connectKey);
    if (rc < 0) {
        WRITE_LOG(LOG_DEBUG, "Initial failed rc:%d", rc);
    }
    // server is not running, "hdc start [-r]" and "hdc kill -r" will start server directly.
    if (serverStatus == 0) {
        HdcServer::PullupServer(channelHostPort.c_str());
        return true;
    }
    // server is running
    if (isRestart) { // "hdc start -r": kill and restart server.
        if (!KillMethodByUv(true)) {
            return false;
        }
        HdcServer::PullupServer(channelHostPort.c_str());
    }
    return true;
}

bool HdcClient::KillMethodByUv(bool isStart)
{
    uint32_t pid = GetLastPID();
    if (pid == 0) {
        Base::PrintMessage(TERMINAL_HDC_PROCESS_FAILED.c_str());
        return false;
    }
    int rc = uv_kill(pid, SIGKILL);
    if (rc == 0) {
        if (isStart) {
            return true;
        }
        Base::PrintMessage("Kill server finish");
    } else {
        constexpr int size = 1024;
        char buf[size] = { 0 };
        uv_strerror_r(rc, buf, size);
        Base::PrintMessage("Kill server failed %s", buf);
        return false;
    }
    return true;
}

bool HdcClient::KillServer(const string &cmd)
{
    int serverStatus = Base::ProgramMutex(true);
    if (serverStatus < 0) {
        WRITE_LOG(LOG_FATAL, "get server status failed, serverStatus:%d", serverStatus);
        return false;
    }

    // server is running
    if (serverStatus != 0 && !KillMethodByUv(false)) {
        return false;
    }

    // server need to restart
    if (cmd.find(" -r") != std::string::npos) {
        string connectKey;
        HdcServer::PullupServer(channelHostPort.c_str());
        uv_sleep(START_SERVER_FOR_CLIENT_TIME);
    }
    return true;
}

void HdcClient::DoCtrlServiceWork(uv_check_t *handle)
{
    HdcClient *thisClass = (HdcClient *)handle->data;
    CALLSTAT_GUARD(thisClass->loopMainStatus, handle->loop, "HdcClient::DoCtrlServiceWork");
    string &strCmd = thisClass->command;
    if (!strncmp(thisClass->command.c_str(), CMDSTR_SERVICE_START.c_str(), CMDSTR_SERVICE_START.size())) {
        thisClass->StartServer(strCmd);
    } else if (!strncmp(thisClass->command.c_str(), CMDSTR_KILLALL_SUB.c_str(),
                        CMDSTR_KILLALL_SUB.size())) {
        SubserverManager::KillAllSubservers();
        Base::PrintMessage("Kill subservers finish");
    } else if (!strncmp(thisClass->command.c_str(), CMDSTR_SERVICE_KILL.c_str(), CMDSTR_SERVICE_KILL.size())) {
        thisClass->KillServer(strCmd);
        // clang-format off
    } else if (!strncmp(thisClass->command.c_str(), CMDSTR_GENERATE_KEY.c_str(), CMDSTR_GENERATE_KEY.size()) &&
                strCmd.find(" ") != std::string::npos) {
        // clang-format on
        string keyPath = strCmd.substr(CMDSTR_GENERATE_KEY.size() + 1, strCmd.size());
        HdcAuth::GenerateKey(keyPath.c_str());
    } else {
        Base::PrintMessage("Unknown command");
    }
    Base::TryCloseHandle((const uv_handle_t *)handle);
}

int HdcClient::CtrlServiceWork(const char *commandIn)
{
    command = commandIn;
    ctrlServerWork.data = this;
    uv_check_init(loopMain, &ctrlServerWork);
    uv_check_start(&ctrlServerWork, DoCtrlServiceWork);
    uv_run(loopMain, UV_RUN_NOWAIT);
    return 0;
}

string HdcClient::AutoConnectKey(string &doCommand, const string &preConnectKey) const
{
    string key = preConnectKey;
    bool isNoTargetCommand = false;
    vector<string> vecNoConnectKeyCommand;
    vecNoConnectKeyCommand.push_back(CMDSTR_SOFTWARE_VERSION);
    vecNoConnectKeyCommand.push_back(CMDSTR_SOFTWARE_HELP);
    vecNoConnectKeyCommand.push_back(CMDSTR_TARGET_DISCOVER);
#ifdef HOST_OHOS
    vecNoConnectKeyCommand.push_back(CMDSTR_SERVICE_KILL);
#endif
    vecNoConnectKeyCommand.push_back(CMDSTR_SPAWN_SUB);
    vecNoConnectKeyCommand.push_back(CMDSTR_KILLALL_SUB);
    vecNoConnectKeyCommand.push_back(CMDSTR_LIST_TARGETS);
    vecNoConnectKeyCommand.push_back(CMDSTR_CHECK_SERVER);
    vecNoConnectKeyCommand.push_back(CMDSTR_CONNECT_TARGET);
    vecNoConnectKeyCommand.push_back(CMDSTR_TARGET_RECONNECT);
    vecNoConnectKeyCommand.push_back(CMDSTR_CHECK_DEVICE);
    vecNoConnectKeyCommand.push_back(CMDSTR_WAIT_FOR);
    vecNoConnectKeyCommand.push_back(CMDSTR_FORWARD_FPORT + " ls");
    vecNoConnectKeyCommand.push_back(CMDSTR_FORWARD_FPORT + " rm");
    for (string v : vecNoConnectKeyCommand) {
        if (!doCommand.compare(0, v.size(), v)) {
            isNoTargetCommand = true;
            break;
        }
    }
    if (isNoTargetCommand) {
        if (this->command != CMDSTR_WAIT_FOR) {
            key = "";
        }
    } else {
        if (!preConnectKey.size()) {
            key = CMDSTR_CONNECT_ANY;
        }
    }
    return key;
}

#ifdef _WIN32
static void ReadFileThreadFunc(void* arg)
{
    char buffer[BUF_SIZE_DEFAULT] = { 0 };
    DWORD bytesRead = 0;

    HANDLE* read = reinterpret_cast<HANDLE*>(arg);
    while (true) {
        if (!ReadFile(*read, buffer, BUF_SIZE_DEFAULT - 1, &bytesRead, NULL)) {
            break;
        }
        string str = std::to_string(bytesRead);
        const char* zero = "0";
        if (!strncmp(zero, str.c_str(), strlen(zero))) {
            return;
        }
        printf("%s", buffer);
        if (memset_s(buffer, sizeof(buffer), 0, sizeof(buffer)) != EOK) {
            return;
        }
    }
}

string HdcClient::GetHilogPath()
{
    string hdcPath = Base::GetHdcAbsolutePath();
    int index = hdcPath.find_last_of(Base::GetPathSep());
    string exePath = hdcPath.substr(0, index) + Base::GetPathSep() + HILOG_NAME;

    return exePath;
}

bool HdcClient::CreatePipePair(HANDLE *hParentRead, HANDLE *hSubWrite, HANDLE *hSubRead, HANDLE *hParentWrite,
    SECURITY_ATTRIBUTES *sa)
{
    if (!CreatePipe(hParentRead, hSubWrite, sa, 0)) {
        return false;
    }
    if (!CreatePipe(hSubRead, hParentWrite, sa, 0)) {
        CloseHandle(*hParentRead);
        CloseHandle(*hSubWrite);
        return false;
    }
    if (!SetHandleInformation(*hParentRead, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(*hParentWrite, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(*hParentRead);
        CloseHandle(*hSubWrite);
        CloseHandle(*hSubRead);
        CloseHandle(*hParentWrite);
        return false;
    }
    return true;
}

bool HdcClient::CreateChildProcess(HANDLE hSubWrite, HANDLE hSubRead, PROCESS_INFORMATION *pi, const string& cmd)
{
    STARTUPINFO si;
    bool ret = false;

    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    GetStartupInfo(&si);
    si.hStdError = hSubWrite;
    si.hStdOutput = hSubWrite;
    si.hStdInput = hSubRead;
    si.wShowWindow = SW_HIDE;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

    do {
        const char *msg = cmd.c_str();
        char buffer[BUF_SIZE_SMALL] = {0};
        if (strcpy_s(buffer, sizeof(buffer), msg) != EOK) {
            break;
        }
        const string exePath = GetHilogPath();
        if (!CreateProcess(_T(exePath.c_str()), _T(buffer), NULL, NULL, true, NULL, NULL, NULL, &si, pi)) {
            WRITE_LOG(LOG_INFO, "create process failed, error:%d", GetLastError());
            break;
        }
        ret = true;
    } while (0);

    return ret;
}

void HdcClient::RunCommandWin32(const string& cmd)
{
    HANDLE hSubWrite;
    HANDLE hParentRead;
    HANDLE hParentWrite;
    HANDLE hSubRead;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = true;

    if (!CreatePipePair(&hParentRead, &hSubWrite, &hSubRead, &hParentWrite, &sa)) {
        return;
    }

    if (!CreateChildProcess(hSubWrite, hSubRead, &pi, cmd)) {
        CloseHandle(hSubWrite);
        CloseHandle(hParentRead);
        CloseHandle(hParentWrite);
        CloseHandle(hSubRead);
    } else {
        auto thread = std::thread([&hParentRead]() {
            ReadFileThreadFunc(&hParentRead);
        });
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(hSubWrite);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        thread.join();
        CloseHandle(hParentRead);
        CloseHandle(hParentWrite);
        CloseHandle(hSubRead);
    }
}
#else
void HdcClient::RunCommand(const string& cmd)
{
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(cmd.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        return;
    }
    char resultBufShell[BUF_SIZE_DEFAULT] = {0};
    while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) {
        printf("%s", resultBufShell);
        if (memset_s(resultBufShell, sizeof(resultBufShell), 0, sizeof(resultBufShell)) != EOK) {
            break;
        }
    }
    pclose(procFileInfo);
}
#endif

void HdcClient::RunExecuteCommand(const string& cmd)
{
    if (FindCommandInject(cmd)) {
        printf("Incorrect command option\n");
        return;
    }
#ifdef _WIN32
    RunCommandWin32(cmd);
#else
    RunCommand(cmd);
#endif
}

bool IsCaptureCommand(const string& cmd)
{
    int index = string(CMDSTR_HILOG).length();
    int length = cmd.length();
    const string captureOption = "parse";
    while (index < length) {
        if (cmd[index] == ' ') {
            index++;
            continue;
        }
        if (!strncmp(cmd.c_str() + index, captureOption.c_str(), captureOption.size())) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

#ifdef __OHOS__
static int ReportCommandEvent(const string &commandIn, bool isIntercepted)
{
#ifdef HDC_SUPPORT_REPORT_COMMAND_EVENT
    if (!DelayedSingleton<CommandEventReport>::GetInstance()->ReportCommandEvent(
        commandIn, Base::GetCaller(), isIntercepted)) {
        WRITE_LOG(LOG_FATAL,
            "[E00C002]Execution intercepted due to inaccessibility of reporting command event.");
        return ERR_GENERIC;
    }
#endif

    if (isIntercepted) {
        WRITE_LOG(LOG_FATAL, "[E00C001]Operation restricted by the organization.");
        return ENTERPRISE_HDC_DISABLE_ERR;
    }
    return RET_SUCCESS;
}
#endif

int HdcClient::ExecuteCommand(const string &commandIn)
{
    char ip[BUF_SIZE_TINY] = "";
    int ret = 0;
#ifdef __OHOS__
    bool isIntercepted = IsNeedInterceptCommand();
    ret = ReportCommandEvent(commandIn, isIntercepted);
    if (ret != RET_SUCCESS) {
        return ret;
    }
#endif
    uint16_t port = 0;
    ret = Base::ConnectKey2IPPort(channelHostPort.c_str(), ip, &port, sizeof(ip));
    if (ret < 0) {
#ifndef __OHOS__
        WRITE_LOG(LOG_FATAL, "ConnectKey2IPPort %s failed with %d",
                  channelHostPort.c_str(), ret);
        return -1;
#endif
    }
    if (!strncmp(commandIn.c_str(), CMDSTR_HILOG.c_str(), CMDSTR_HILOG.size()) &&
        IsCaptureCommand(commandIn)) {
        RunExecuteCommand(commandIn);
        return 0;
    }
    if (!strncmp(commandIn.c_str(), CMDSTR_FILE_SEND.c_str(), CMDSTR_FILE_SEND.size()) ||
        !strncmp(commandIn.c_str(), CMDSTR_FILE_RECV.c_str(), CMDSTR_FILE_RECV.size())) {
        WRITE_LOG(LOG_DEBUG, "Set file send mode");
        channel->remote = RemoteType::REMOTE_FILE;
    } else if (!strncmp(commandIn.c_str(), CMDSTR_APP_INSTALL.c_str(), CMDSTR_APP_INSTALL.size())) {
        channel->remote = RemoteType::REMOTE_APP;
    }
    command = commandIn;
    connectKey = AutoConnectKey(command, connectKey);
#ifdef __OHOS__
    AdminChannel(OP_UPDATE, channel->channelId, channel);
    if (channel->isUds) {
        ConnectUdsServerForClient();
    } else {
        ConnectServerForClient(ip, port);
    }
#else
    ConnectServerForClient(ip, port);
#endif
    uv_timer_init(loopMain, &waitTimeDoCmd);
    waitTimeDoCmd.data = this;
    uv_timer_start(&waitTimeDoCmd, CommandWorker, UV_START_TIMEOUT, UV_START_REPEAT);
    WorkerPendding();
    return 0;
}

int HdcClient::Initial(const string &connectKeyIn)
{
    connectKey = connectKeyIn;
#ifdef __OHOS__
    if (channel->isUds) {
        return 0;
    }
#endif
    if (!channelHostPort.size() || !channelHost.size() || !channelPort) {
        WRITE_LOG(LOG_FATAL, "Listen string initial failed");
        return ERR_PARM_FAIL;
    }
    return 0;
}

#ifdef __OHOS__
int HdcClient::ConnectUdsServerForClient()
{
    if (uv_is_closing((const uv_handle_t *)&channel->hWorkUds)) {
        WRITE_LOG(LOG_FATAL, "ConnectServerForClient uv_is_closing");
        return ERR_SOCKET_FAIL;
    }
    WRITE_LOG(LOG_DEBUG, "Try to connect uds");
    uv_connect_t *conn = new(std::nothrow) uv_connect_t();
    if (conn == nullptr) {
        WRITE_LOG(LOG_FATAL, "ConnectServerForClient new conn failed");
        return ERR_GENERIC;
    }
    conn->data = this;
    udsConnectRetryCount = 0;
    uv_timer_init(loopMain, &retryUdsConnTimer);
    retryUdsConnTimer.data = this;
    
    uv_pipe_connect(conn, (uv_pipe_t *)&channel->hWorkUds, UDS_PATH.c_str(), ConnectUds);
    return 0;
}
#endif

int HdcClient::ConnectServerForClient(const char *ip, uint16_t port)
{
    if (uv_is_closing((const uv_handle_t *)&channel->hWorkTCP)) {
        WRITE_LOG(LOG_FATAL, "ConnectServerForClient uv_is_closing");
        return ERR_SOCKET_FAIL;
    }
    WRITE_LOG(LOG_DEBUG, "Try to connect %s:%d", ip, port);
    uv_connect_t *conn = new(std::nothrow) uv_connect_t();
    if (conn == nullptr) {
        WRITE_LOG(LOG_FATAL, "ConnectServerForClient new conn failed");
        return ERR_GENERIC;
    }
    conn->data = this;
    tcpConnectRetryCount = 0;
    uv_timer_init(loopMain, &retryTcpConnTimer);
    retryTcpConnTimer.data = this;
    if (strchr(ip, '.')) {
        isIpV4 = true;
        std::string s = ip;
        size_t index = s.find(IPV4_MAPPING_PREFIX);
        size_t size = IPV4_MAPPING_PREFIX.size();
        if (index != std::string::npos) {
            s = s.substr(index + size);
        }
        WRITE_LOG(LOG_DEBUG, "ConnectServerForClient ipv4 %s:%d", s.c_str(), port);
        uv_ip4_addr(s.c_str(), port, &destv4);
        uv_tcp_connect(conn, (uv_tcp_t *)&channel->hWorkTCP, (const struct sockaddr *)&destv4, Connect);
    } else {
        isIpV4 = false;
        WRITE_LOG(LOG_DEBUG, "ConnectServerForClient ipv6 %s:%d", ip, port);
        uv_ip6_addr(ip, port, &dest);
        uv_tcp_connect(conn, (uv_tcp_t *)&channel->hWorkTCP, (const struct sockaddr *)&dest, Connect);
    }
    return 0;
}

void HdcClient::CommandWorker(uv_timer_t *handle)
{
    const uint16_t maxWaitRetry = 1200; // client socket try 12s
    HdcClient *thisClass = (HdcClient *)handle->data;
    CALLSTAT_GUARD(thisClass->loopMainStatus, handle->loop, "HdcClient::CommandWorker");
    if (++thisClass->debugRetryCount > maxWaitRetry) {
        uv_timer_stop(handle);
        uv_stop(thisClass->loopMain);
        WRITE_LOG(LOG_DEBUG, "Connect server failed");
        fprintf(stderr, "Connect server failed\n");
        return;
    }
    if (!thisClass->channel->handshakeOK) {
        return;
    }
    uv_timer_stop(handle);
#ifdef HOST_OHOS
    if (!strncmp(thisClass->command.c_str(), CMDSTR_SERVICE_KILL.c_str(),
        CMDSTR_SERVICE_KILL.size()) && !thisClass->channel->isSupportedKillServerCmd) {
        WRITE_LOG(LOG_DEBUG, "uv_kill server");
        thisClass->CtrlServiceWork(CMDSTR_SERVICE_KILL.c_str());
        return;
    }
#endif
    WRITE_LOG(LOG_DEBUG, "Connect server successful");
    bool closeInput = false;
    if (!HostUpdater::ConfirmCommand(thisClass->command, closeInput)) {
        uv_timer_stop(handle);
        uv_stop(thisClass->loopMain);
        if (Base::GetCaller() == Base::Caller::SERVER) {
            WRITE_LOG(LOG_DEBUG, "Cmd \'%s\' has been canceld", Hdc::MaskString(thisClass->command).c_str());
        } else {
            WRITE_LOG(LOG_DEBUG, "Cmd \'%s\' has been canceld", thisClass->command.c_str());
        }
        return;
    }
    while (closeInput) {
#ifndef _WIN32
        if (tcgetattr(STDIN_FILENO, &thisClass->terminalState)) {
            break;
        }
        termios tio;
        if (tcgetattr(STDIN_FILENO, &tio)) {
            break;
        }
        cfmakeraw(&tio);
        tio.c_cc[VTIME] = 0;
        tio.c_cc[VMIN] = 1;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio);
        g_terminalStateChange = true;
#endif
        break;
    }
    thisClass->Send(thisClass->channel->channelId,
                    const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(thisClass->command.c_str())),
                    thisClass->command.size() + 1);
}

void HdcClient::AllocStdbuf(uv_handle_t *handle, size_t sizeWanted, uv_buf_t *buf)
{
    if (sizeWanted <= 0) {
        return;
    }
    HChannel context = (HChannel)handle->data;
    if (!context) {
        WRITE_LOG(LOG_WARN, "AllocStdbuf: invalid context");
        return;
    }
    int availSize = static_cast<int>(strnlen(context->bufStd, sizeof(context->bufStd)));
    const int reserveSize = 2; // reserve bytes for safety
    if (availSize >= static_cast<int>(sizeof(context->bufStd)) - reserveSize) {
        WRITE_LOG(LOG_WARN, "AllocStdbuf: buffer overflow detected, availSize: %d, maxSize: %zu", availSize,
            sizeof(context->bufStd) - reserveSize);
        buf->len = 0;
        return;
    }
    buf->base = (char *)context->bufStd + availSize;
    buf->len = sizeof(context->bufStd) - availSize - reserveSize;
}

void HdcClient::ReadStd(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    HChannel hChannel = (HChannel)stream->data;
    HdcClient *thisClass = (HdcClient *)hChannel->clsChannel;
    CALLSTAT_GUARD(thisClass->loopMainStatus, stream->loop, "HdcClient::ReadStd");
    if (nread <= 0) {
        WRITE_LOG(LOG_FATAL, "ReadStd error nread:%zd", nread);
        return;  // error
    }
    thisClass->ForwardStdin(hChannel, reinterpret_cast<uint8_t *>(buf->base), static_cast<size_t>(nread));
    (void)memset_s(hChannel->bufStd, sizeof(hChannel->bufStd), 0, sizeof(hChannel->bufStd));
}

void HdcClient::ForwardStdin(HChannel hChannel, uint8_t *data, size_t size)
{
    if (!hChannel->handshakeOK) {
        WRITE_LOG(LOG_DEBUG, "ForwardStdin handshake not ready");
        return;
    }
    if (data == nullptr || size == 0 || size > static_cast<size_t>(INT_MAX)) {
        WRITE_LOG(LOG_WARN, "ForwardStdin invalid input size:%zu", size);
        return;
    }
    Send(hChannel->channelId, data, static_cast<int>(size));
}

#ifndef _WIN32
bool HdcClient::PollAndForwardStdin(HChannel hChannel)
{
    pollfd stdinPoll = { STDIN_FILENO, POLLIN, 0 };
    int rc;
    do {
        rc = poll(&stdinPoll, 1, STDIN_POLL_TIMEOUT_MS);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        WRITE_LOG(LOG_WARN, "Poll stdin failed:%d", errno);
        return false;
    }
    if (rc == 0 || (stdinPoll.revents & POLLIN) == 0) {
        return true;
    }

    uint8_t input[BUF_SIZE_SMALL] = { 0 };
    ssize_t nread;
    do {
        nread = read(STDIN_FILENO, input, sizeof(input));
    } while (nread < 0 && errno == EINTR);
    if (nread < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        WRITE_LOG(LOG_WARN, "Read stdin failed:%d", errno);
        return false;
    }
    if (nread > 0) {
        ForwardStdin(hChannel, input, static_cast<size_t>(nread));
    }
    return true;
}

int HdcClient::WriteShellOutput(HChannel hChannel, const string &output)
{
    bool interactive = bShellInteractive && hChannel->stdinTty.data != nullptr &&
        !uv_is_closing(reinterpret_cast<uv_handle_t *>(&hChannel->stdinTty));
    if (!interactive) {
        return WriteStdoutAll(output.data(), output.size()) ? 0 : ERR_IO_FAIL;
    }

    int stopRc = uv_read_stop(reinterpret_cast<uv_stream_t *>(&hChannel->stdinTty));
    if (stopRc < 0) {
        WRITE_LOG(LOG_WARN, "Stop stdin read failed:%s", uv_strerror(stopRc));
        return ERR_IO_FAIL;
    }

    bool writeOk = true;
    size_t offset = 0;
    while (offset < output.size()) {
        size_t bytesUntilPoll = STDIN_POLL_INTERVAL_BYTES - stdinBytesSincePoll;
        size_t size = std::min(bytesUntilPoll, output.size() - offset);
        if (!WriteStdoutAll(output.data() + offset, size)) {
            writeOk = false;
            break;
        }
        offset += size;
        stdinBytesSincePoll += size;
        if (stdinBytesSincePoll == STDIN_POLL_INTERVAL_BYTES) {
            stdinBytesSincePoll = 0;
            writeOk = PollAndForwardStdin(hChannel);
            if (!writeOk) {
                break;
            }
        }
    }

    int startRc = uv_read_start(reinterpret_cast<uv_stream_t *>(&hChannel->stdinTty), AllocStdbuf, ReadStd);
    if (startRc < 0) {
        WRITE_LOG(LOG_WARN, "Restart stdin read failed:%s", uv_strerror(startRc));
        writeOk = false;
    }
    return writeOk ? 0 : ERR_IO_FAIL;
}
#endif

void HdcClient::ModifyTty(bool setOrRestore, uv_tty_t *tty)
{
    if (setOrRestore) {
#ifdef _WIN32
        uv_tty_set_mode(tty, UV_TTY_MODE_RAW);
#else
        if (tcgetattr(STDIN_FILENO, &terminalState)) {
            return;
        }
        termios tio;
        if (tcgetattr(STDIN_FILENO, &tio)) {
            return;
        }
        cfmakeraw(&tio);
        tio.c_cc[VTIME] = 0;
        tio.c_cc[VMIN] = 1;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio);
#endif
    } else {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminalState);
#endif
    }
}

void HdcClient::SetShellInteractive()
{
    bShellInteractive = command == CMDSTR_SHELL;
    if (command.rfind(CMDSTR_SHELL_EX, 0) != 0) {
        return;
    }

    bShellInteractive = true;
    int argc = 0;
    char **argv = Base::SplitCommandToArgs(command.c_str(), &argc);
    int skipNext = 0;
    for (int i = 0; i < argc; i++) {
        if (skipNext > 0) {
            skipNext--;
            continue;
        }
        if (std::strcmp(argv[i], CMDSTR_SHELL.c_str()) == 0) {
            continue;
        }
        if (std::strcmp(argv[i], "-b") == 0) {
            skipNext++;
            continue;
        }
        bShellInteractive = false;
    }
    delete[](reinterpret_cast<char *>(argv));
}

void HdcClient::BindLocalStd(HChannel hChannel)
{
    SetShellInteractive();
    if (bShellInteractive && uv_guess_handle(STDIN_FILENO) != UV_TTY) {
        WRITE_LOG(LOG_WARN, "Not support stdio TTY mode");
        return;
    }

#ifndef _WIN32
    if (!bShellInteractive) {
        return;
    }
#endif

    WRITE_LOG(LOG_DEBUG, "setup stdio TTY mode");
    if (uv_tty_init(loopMain, &hChannel->stdoutTty, STDOUT_FILENO, 0) != 0) {
        WRITE_LOG(LOG_DEBUG, "uv_tty_init stdout failed");
        return;
    }
    if (uv_tty_init(loopMain, &hChannel->stdinTty, STDIN_FILENO, 1) != 0) {
        WRITE_LOG(LOG_DEBUG, "uv_tty_init stdin failed");
        uv_close((uv_handle_t *)&hChannel->stdoutTty, nullptr);
        return;
    }
    hChannel->stdoutTty.data = hChannel;
    ++hChannel->uvHandleRef;
    hChannel->stdinTty.data = hChannel;
    ++hChannel->uvHandleRef;
#ifndef _WIN32
    SetStdoutBlocking(&hChannel->stdoutTty);
#endif
    if (bShellInteractive) {
        WRITE_LOG(LOG_DEBUG, "uv_tty_init uv_tty_set_mode");
        ModifyTty(true, &hChannel->stdinTty);
        uv_read_start((uv_stream_t *)&hChannel->stdinTty, AllocStdbuf, ReadStd);
    }
}

#ifdef __OHOS__
void HdcClient::ConnectUds(uv_connect_t *connection, int status)
{
    WRITE_LOG(LOG_DEBUG, "Enter ConnectUds, status:%d", status);
    HdcClient *thisClass = (HdcClient *)connection->data;
    CALLSTAT_GUARD(thisClass->loopMainStatus, connection->handle->loop, "HdcClient::Connect");
    delete connection;
    HChannel hChannel = reinterpret_cast<HChannel>(thisClass->channel);
    if (uv_is_closing((const uv_handle_t *)&hChannel->hWorkUds)) {
        WRITE_LOG(LOG_DEBUG, "uv_is_closing...");
        thisClass->FreeChannel(hChannel->channelId);
        return;
    }

    // connect success
    if (status == 0) {
        thisClass->BindLocalStd(hChannel);
        Base::SetUdsOptions((uv_pipe_t *)&hChannel->hWorkUds);
        WRITE_LOG(LOG_DEBUG, "uv_read_start");
        uv_read_start((uv_stream_t *)&hChannel->hWorkUds, AllocCallback, ReadStream);
        return;
    }

    // connect failed, start timer and retry
    WRITE_LOG(LOG_DEBUG, "retry count:%d", thisClass->udsConnectRetryCount);
    if (thisClass->udsConnectRetryCount >= TCP_CONNECT_MAX_RETRY_COUNT) {
        WRITE_LOG(LOG_DEBUG, "stop retry for connect");
        thisClass->FreeChannel(hChannel->channelId);
        return;
    }
    thisClass->udsConnectRetryCount++;
    uv_timer_start(&(thisClass->retryUdsConnTimer), thisClass->RetryUdsConnectWorker, TCP_CONNECT_RETRY_TIME_MS, 0);
}
#endif

void HdcClient::Connect(uv_connect_t *connection, int status)
{
    WRITE_LOG(LOG_DEBUG, "Enter Connect, status:%d", status);
    HdcClient *thisClass = (HdcClient *)connection->data;
    CALLSTAT_GUARD(thisClass->loopMainStatus, connection->handle->loop, "HdcClient::Connect");
    delete connection;
    HChannel hChannel = reinterpret_cast<HChannel>(thisClass->channel);
    if (uv_is_closing((const uv_handle_t *)&hChannel->hWorkTCP)) {
        WRITE_LOG(LOG_DEBUG, "uv_is_closing...");
        thisClass->FreeChannel(hChannel->channelId);
        return;
    }

    // connect success
    if (status == 0) {
        thisClass->BindLocalStd(hChannel);
        Base::SetTcpOptions((uv_tcp_t *)&hChannel->hWorkTCP);
        WRITE_LOG(LOG_DEBUG, "uv_read_start");
        uv_read_start((uv_stream_t *)&hChannel->hWorkTCP, AllocCallback, ReadStream);
        return;
    }

    // connect failed, start timer and retry
    WRITE_LOG(LOG_DEBUG, "retry count:%d", thisClass->tcpConnectRetryCount);
    if (thisClass->tcpConnectRetryCount >= TCP_CONNECT_MAX_RETRY_COUNT) {
        WRITE_LOG(LOG_DEBUG, "stop retry for connect");
        thisClass->FreeChannel(hChannel->channelId);
        return;
    }
    thisClass->tcpConnectRetryCount++;
    uv_timer_start(&(thisClass->retryTcpConnTimer), thisClass->RetryTcpConnectWorker, TCP_CONNECT_RETRY_TIME_MS, 0);
}

#ifdef __OHOS__
void HdcClient::RetryUdsConnectWorker(uv_timer_t *handle)
{
    HdcClient *thisClass = (HdcClient *)handle->data;
    HChannel hChannel = reinterpret_cast<HChannel>(thisClass->channel);
    CALLSTAT_GUARD(thisClass->loopMainStatus, handle->loop, "HdcClient::RetryUdsConnectWorker");
    uv_connect_t *connection = new(std::nothrow) uv_connect_t();
    if (connection == nullptr) {
        WRITE_LOG(LOG_FATAL, "RetryUdsConnectWorker new conn failed");
        thisClass->FreeChannel(hChannel->channelId);
        return;
    }
    connection->data = thisClass;
    WRITE_LOG(LOG_DEBUG, "RetryUdsConnectWorker start tcp connect");
    uv_pipe_connect(connection, &(thisClass->channel->hWorkUds), UDS_PATH.c_str(), thisClass->ConnectUds);
}
#endif

void HdcClient::RetryTcpConnectWorker(uv_timer_t *handle)
{
    HdcClient *thisClass = (HdcClient *)handle->data;
    HChannel hChannel = reinterpret_cast<HChannel>(thisClass->channel);
    CALLSTAT_GUARD(thisClass->loopMainStatus, handle->loop, "HdcClient::RetryTcpConnectWorker");
    uv_connect_t *connection = new(std::nothrow) uv_connect_t();
    if (connection == nullptr) {
        WRITE_LOG(LOG_FATAL, "RetryTcpConnectWorker new conn failed");
        thisClass->FreeChannel(hChannel->channelId);
        return;
    }
    connection->data = thisClass;
    WRITE_LOG(LOG_DEBUG, "RetryTcpConnectWorker start tcp connect");
    if (thisClass->isIpV4) {
        uv_tcp_connect(connection, &(thisClass->channel->hWorkTCP),
            (const struct sockaddr *)&(thisClass->destv4), thisClass->Connect);
    } else {
        uv_tcp_connect(connection, &(thisClass->channel->hWorkTCP),
            (const struct sockaddr *)&(thisClass->dest), thisClass->Connect);
    }
}

int HdcClient::ValidateHandshakeBanner(HChannel hChannel, const uint8_t *buf, const int bytesIO)
{
    if (bytesIO < static_cast<int>(offsetof(struct ChannelHandShake, version))) {
        WRITE_LOG(LOG_WARN, "PreHandshake buf too short, bytesIO:%d need:%d",
            bytesIO, static_cast<int>(offsetof(struct ChannelHandShake, version)));
        return ERR_BUF_SIZE;
    }
    ChannelHandShake *hShake = reinterpret_cast<ChannelHandShake *>(const_cast<uint8_t *>(buf));
    if (strncmp(hShake->banner, HANDSHAKE_MESSAGE.c_str(), HANDSHAKE_MESSAGE.size())) {
        hChannel->availTailIndex = 0;
        WRITE_LOG(LOG_DEBUG, "Channel Hello failed");
        return ERR_BUF_CHECK;
    }
    hChannel->isStableBuf = (hShake->banner[BANNER_FEATURE_TAG_OFFSET] != HUGE_BUF_TAG);
#ifdef HOST_OHOS
    hChannel->isSupportedKillServerCmd = (hShake->banner[SERVICE_KILL_OFFSET] == SERVICE_KILL_TAG);
    WRITE_LOG(LOG_DEBUG, "Channel PreHandshake isStableBuf:%d, killflag:%d",
        hChannel->isStableBuf, hChannel->isSupportedKillServerCmd);
#else
    WRITE_LOG(LOG_DEBUG, "Channel PreHandshake isStableBuf:%d", hChannel->isStableBuf);
#endif
    if (this->command == CMDSTR_WAIT_FOR && !connectKey.empty()) {
        hShake->banner[WAIT_TAG_OFFSET] = WAIT_DEVICE_TAG;
    }
    return RET_SUCCESS;
}

void HdcClient::SyncChannelId(HChannel hChannel, const uint8_t *buf)
{
    ChannelHandShake *hShake = reinterpret_cast<ChannelHandShake *>(const_cast<uint8_t *>(buf));
    uint32_t unOld = hChannel->channelId;
    hChannel->channelId = ntohl(hShake->channelId);
    AdminChannel(OP_UPDATE, unOld, hChannel);
    WRITE_LOG(LOG_DEBUG, "Client channel handshake finished, use connectkey:%s",
              Hdc::MaskString(connectKey).c_str());
}

int HdcClient::FillConnectKeyAndCheckVersion(uint32_t channelId, ChannelHandShake *hShake)
{
    if (memset_s(hShake->connectKey, sizeof(hShake->connectKey), 0, sizeof(hShake->connectKey)) != EOK
        || memcpy_s(hShake->connectKey, sizeof(hShake->connectKey), connectKey.c_str(), connectKey.size()) != EOK) {
        WRITE_LOG(LOG_DEBUG, "Channel Hello failed");
        return ERR_BUF_COPY;
    }
#ifdef HDC_VERSION_CHECK
    if (!isCheckVersionCmd) {
        string clientVer = Base::GetVersion() + HDC_MSG_HASH;
        string serverVer(hShake->version, strnlen(hShake->version, BUF_SIZE_TINY));
        if (clientVer != serverVer) {
            if (serverVer.size() >= Base::GetVersion().size()) {
                serverVer = serverVer.substr(0, Base::GetVersion().size());
            }
            WRITE_LOG(LOG_FATAL, "Client version:%s, server version:%s", clientVer.c_str(), serverVer.c_str());
            return ERR_CHECK_VERSION;
        }
    }
    Send(channelId, reinterpret_cast<uint8_t *>(hShake), sizeof(ChannelHandShake));
#else
    Send(channelId, reinterpret_cast<uint8_t *>(hShake), offsetof(struct ChannelHandShake, version));
#endif
    return RET_SUCCESS;
}

void HdcClient::FinalizeHandshake(HChannel hChannel, ChannelHandShake *hShake)
{
    hChannel->handshakeOK = true;
#ifdef HDC_CHANNEL_KEEP_ALIVE
    Send(hChannel->channelId,
         reinterpret_cast<uint8_t *>(const_cast<char*>(CMDSTR_INNER_ENABLE_KEEPALIVE.c_str())),
         CMDSTR_INNER_ENABLE_KEEPALIVE.size());
#endif
}

int HdcClient::PreHandshake(HChannel hChannel, const uint8_t *buf, const int bytesIO)
{
    int ret = ValidateHandshakeBanner(hChannel, buf, bytesIO);
    if (ret != RET_SUCCESS) {
        return ret;
    }
    SyncChannelId(hChannel, buf);
    ChannelHandShake *hShake = reinterpret_cast<ChannelHandShake *>(const_cast<uint8_t *>(buf));
    ret = FillConnectKeyAndCheckVersion(hChannel->channelId, hShake);
    if (ret != RET_SUCCESS) {
        hChannel->availTailIndex = 0;
        return ret;
    }
    FinalizeHandshake(hChannel, hShake);
    return RET_SUCCESS;
}

// read serverForClient(server)TCP data
bool HdcClient::DispatchRemoteTask(HChannel hChannel, uint16_t cmd, uint8_t *buf, int bytesIO)
{
    if (hChannel->remote <= RemoteType::REMOTE_NONE || !IsOffset(cmd)) {
        return false;
    }
    if (hChannel->remote == RemoteType::REMOTE_FILE) {
        if (fileTask == nullptr) {
            HTaskInfo hTaskInfo = GetRemoteTaskInfo(hChannel);
            hTaskInfo->masterSlave = (cmd == CMD_FILE_INIT);
            fileTask = std::make_unique<HdcFile>(hTaskInfo);
        }
        if (!fileTask->CommandDispatch(cmd, buf + sizeof(uint16_t), bytesIO - sizeof(uint16_t))) {
            fileTask->TaskFinish();
        }
    }
    if (hChannel->remote == RemoteType::REMOTE_APP) {
        if (appTask == nullptr) {
            HTaskInfo hTaskInfo = GetRemoteTaskInfo(hChannel);
            hTaskInfo->masterSlave = (cmd == CMD_APP_INIT);
            appTask = std::make_unique<HdcHostApp>(hTaskInfo);
        }
        if (!appTask->CommandDispatch(cmd, buf + sizeof(uint16_t), bytesIO - sizeof(uint16_t))) {
            appTask->TaskFinish();
        }
    }
    return true;
}

int HdcClient::ReadChannel(HChannel hChannel, uint8_t *buf, const int bytesIO)
{
    if (!hChannel->handshakeOK) {
        return PreHandshake(hChannel, buf, bytesIO);
    }
#ifdef UNIT_TEST
    // Do not output to console when the unit test
    return 0;
#endif
    WRITE_LOG(LOG_DEBUG, "Client ReadChannel :%d", bytesIO);

    uint16_t cmd = 0;
    if (bytesIO >= static_cast<int>(sizeof(uint16_t))) {
        cmd = *reinterpret_cast<uint16_t *>(buf);
    }
    if (cmd == CMD_CHECK_SERVER && isCheckVersionCmd) {
        WRITE_LOG(LOG_DEBUG, "recieve CMD_CHECK_VERSION command");
        string version(reinterpret_cast<char *>(buf + sizeof(uint16_t)), bytesIO - sizeof(uint16_t));
        fprintf(stdout, "Client version:%s, server version:%s\n", Base::GetVersion().c_str(), version.c_str());
        fflush(stdout);
        return 0;
    }
    if (DispatchRemoteTask(hChannel, cmd, buf, bytesIO)) {
        return 0;
    }

    string s(reinterpret_cast<char *>(buf), bytesIO);
    if (WaitFor(s)) {
        return 0;
    }
    if (WaitForSpawn(s)) {
        return 0;
    }
    s = ListTargetsAll(s);
    if (g_show) {
#ifdef _WIN32
        fprintf(stdout, "%s", s.c_str());
        fflush(stdout);
#else
        return WriteShellOutput(hChannel, s);
#endif
    }
    return 0;
}

bool HdcClient::WaitFor(const string &str)
{
    bool wait = false;
    if (!strncmp(this->command.c_str(), CMDSTR_WAIT_FOR.c_str(), CMDSTR_WAIT_FOR.size())) {
        const string waitFor = "[Fail]No any connected target";
        if (!strncmp(str.c_str(), waitFor.c_str(), waitFor.size())) {
            Send(this->channel->channelId, reinterpret_cast<uint8_t *>(const_cast<char *>(this->command.c_str())),
                 this->command.size() + 1);
            constexpr int timeout = 1;
            std::this_thread::sleep_for(std::chrono::seconds(timeout));
            wait = true;
        } else {
            _exit(0);
        }
    }
    return wait;
}

bool HdcClient::WaitForSpawn(const string &str)
{
    bool wait = false;
    if (!strncmp(this->command.c_str(), CMDSTR_SPAWN_SUB.c_str(), CMDSTR_SPAWN_SUB.size())) {
        static int retryCount = 0;
        const string waitFor = "Subserver started, connecting USB";
        if (retryCount++ <= WAIT_FOR_SPAWN_MAX_TIMES && !strncmp(str.c_str(), waitFor.c_str(), waitFor.size())) {
            Send(this->channel->channelId, reinterpret_cast<uint8_t *>(const_cast<char *>(this->command.c_str())),
                 this->command.size() + 1);
            constexpr int timeout = 1;
            std::this_thread::sleep_for(std::chrono::seconds(timeout));
            wait = true;
        } else {
            string msg = str;
            msg.erase(msg.find_last_not_of("\n\r") + 1);
            Base::PrintMessage("%s", msg.c_str());
            fflush(stdout);
            _exit(0);
        }
    }
    return wait;
}

string HdcClient::ListTargetsAll(const string &str)
{
    string all = str;
    const string lists = "list targets -v";
    if (!strncmp(this->command.c_str(), lists.c_str(), lists.size())) {
        UpdateList(str);
        all = Base::ReplaceAll(all, "\n", "\thdc\n");
    } else if (!strncmp(this->command.c_str(), CMDSTR_LIST_TARGETS.c_str(), CMDSTR_LIST_TARGETS.size())) {
        UpdateList(str);
    }
    if (!strncmp(this->command.c_str(), CMDSTR_LIST_TARGETS.c_str(), CMDSTR_LIST_TARGETS.size())) {
        if (g_lists.size() > 0 && !strncmp(str.c_str(), EMPTY_ECHO.c_str(), EMPTY_ECHO.size())) {
            all = "";
        }
    }
    return all;
}

void HdcClient::UpdateList(const string &str)
{
    if (!strncmp(str.c_str(), EMPTY_ECHO.c_str(), EMPTY_ECHO.size())) {
        return;
    }
    vector<string> devs;
    Base::SplitString(str, "\n", devs);
    for (size_t i = 0; i < devs.size(); i++) {
        string::size_type pos = devs[i].find("\t");
        if (pos != string::npos) {
            string key = devs[i].substr(0, pos);
            g_lists[key] = "hdc";
        } else {
            string key = devs[i];
            g_lists[key] = "hdc";
        }
    }
}

bool HdcClient::IsOffset(uint16_t cmd)
{
    return (cmd == CMD_CHECK_SERVER) ||
           (cmd == CMD_FILE_INIT) ||
           (cmd == CMD_FILE_CHECK) ||
           (cmd == CMD_FILE_BEGIN) ||
           (cmd == CMD_FILE_DATA) ||
           (cmd == CMD_FILE_FINISH) ||
           (cmd == CMD_FILE_MODE) ||
           (cmd == CMD_DIR_MODE) ||
           (cmd == CMD_APP_INIT) ||
           (cmd == CMD_APP_CHECK) ||
           (cmd == CMD_APP_BEGIN) ||
           (cmd == CMD_APP_DATA) ||
           (cmd == CMD_APP_FINISH);
}

HTaskInfo HdcClient::GetRemoteTaskInfo(HChannel hChannel)
{
    HTaskInfo hTaskInfo = new TaskInformation();
    hTaskInfo->channelId = hChannel->channelId;
    hTaskInfo->runLoop = loopMain;
    hTaskInfo->runLoopStatus = &loopMainStatus;
    hTaskInfo->serverOrDaemon = true;
    hTaskInfo->channelTask = true;
    hTaskInfo->channelClass = this;
    hTaskInfo->isStableBuf = hChannel->isStableBuf;
    hTaskInfo->isCleared = false;
    return hTaskInfo;
};

#ifdef __OHOS__
// Analyse whether intercept hdc commands
bool HdcClient::IsNeedInterceptCommand()
{
    std::string out;
    if (!SystemDepend::GetDevItem(SYS_PARAM_ENTERPRISE_HDC_DISABLE.c_str(), out)) {
        return false;
    }
    if (out.empty() || out == "false") {
        return false;
    }
    return true;
}
#endif
}  // namespace Hdc
