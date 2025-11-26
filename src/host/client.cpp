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
#include "client.h"
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
    MallocChannel(&channel);  // free by logic
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
    Base::ReadBinFile(path.c_str(), reinterpret_cast<void **>(&pidBuf), BUF_SIZE_TINY);
    int pid = atoi(pidBuf);  // pid  maybe 0
    return pid;
}

bool HdcClient::StartServer(const string &cmd)
{
    int serverStatus = Base::ProgramMutex(SERVER_NAME.c_str(), true);
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

bool HdcClient::ChannelCtrlServer(const string &cmd, string &connectKey)
{
    // new version build channle to send Ctrl command to server
    int serverStatus = Base::ProgramMutex(SERVER_NAME.c_str(), true);
    if (serverStatus < 0) {
        WRITE_LOG(LOG_DEBUG, "get server status failed, serverStatus:%d", serverStatus);
        return false;
    }
    bool isRestart = (cmd.find(" -r") != std::string::npos);
    bool isKill = !strncmp(cmd.c_str(), CMDSTR_SERVICE_KILL.c_str(), CMDSTR_SERVICE_KILL.size());
    bool isStart = !strncmp(cmd.c_str(), CMDSTR_SERVICE_START.c_str(), CMDSTR_SERVICE_START.size());
    if (isKill) {
        Base::PrintMessage("[E002201]Kill server failed, unsupport channel kill.");
        return false;
    }
    if (!isStart) {
        Base::PrintMessage("[E002202]Unsupport command or parameters");
        return false;
    }
    Initial(connectKey);
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
    if (isStart) {
        return true;
    }
    if (rc == 0) {
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
    int serverStatus = Base::ProgramMutex(SERVER_NAME.c_str(), true);
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
    vecNoConnectKeyCommand.push_back(CMDSTR_LIST_TARGETS);
    vecNoConnectKeyCommand.push_back(CMDSTR_CHECK_SERVER);
    vecNoConnectKeyCommand.push_back(CMDSTR_CONNECT_TARGET);
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

#ifdef __OHOS__
    if (isIntercepted) {
        WRITE_LOG(LOG_FATAL, "[E00C001]Operation restricted by the organization.");
        return ENTERPRISE_HDC_DISABLE_ERR;
    }
#endif
    return RET_SUCCESS;
}

int HdcClient::ExecuteCommand(const string &commandIn)
{
    char ip[BUF_SIZE_TINY] = "";
    bool isIntercepted = false;
#ifdef __OHOS__
    isIntercepted = IsNeedInterceptCommand();
#endif
    int ret = ReportCommandEvent(commandIn, isIntercepted);
    if (ret != RET_SUCCESS) {
        return ret;
    }
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
    int availSize = strlen(context->bufStd);
    buf->base = (char *)context->bufStd + availSize;
    buf->len = sizeof(context->bufStd) - availSize - 2;  // reserve 2bytes
}

void HdcClient::ReadStd(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    HChannel hChannel = (HChannel)stream->data;
    HdcClient *thisClass = (HdcClient *)hChannel->clsChannel;
    CALLSTAT_GUARD(thisClass->loopMainStatus, stream->loop, "HdcClient::ReadStd");
    if (!hChannel->handshakeOK) {
        WRITE_LOG(LOG_WARN, "ReadStd handshake not ready");
        return; // if not handshake, do not send the cmd input to server.
    }
    char *cmd = hChannel->bufStd;
    if (nread <= 0) {
        WRITE_LOG(LOG_FATAL, "ReadStd error nread:%zd", nread);
        return;  // error
    }
    thisClass->Send(hChannel->channelId, reinterpret_cast<uint8_t *>(cmd), strlen(cmd));
    (void)memset_s(hChannel->bufStd, sizeof(hChannel->bufStd), 0, sizeof(hChannel->bufStd));
}

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

void HdcClient::BindLocalStd(HChannel hChannel)
{
    if (command == CMDSTR_SHELL) {
        bShellInteractive = true;
    }
    if (bShellInteractive && uv_guess_handle(STDIN_FILENO) != UV_TTY) {
        WRITE_LOG(LOG_WARN, "Not support stdio TTY mode");
        return;
    }

    WRITE_LOG(LOG_DEBUG, "setup stdio TTY mode");
    if (uv_tty_init(loopMain, &hChannel->stdoutTty, STDOUT_FILENO, 0)
        || uv_tty_init(loopMain, &hChannel->stdinTty, STDIN_FILENO, 1)) {
        WRITE_LOG(LOG_DEBUG, "uv_tty_init failed");
        return;
    }
    hChannel->stdoutTty.data = hChannel;
    ++hChannel->uvHandleRef;
    hChannel->stdinTty.data = hChannel;
    ++hChannel->uvHandleRef;
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
std::string HdcClient::CharPointToString(const char *buf, const int size)
{
    if (buf == nullptr || size <= 0) {
        return std::string();
    }
    for (int i = 0; i < size; i++) {
        if (buf[i] == '\0') {
            return string(buf, i);
        }
    }
    return string(buf, size);
}

int HdcClient::PreHandshake(HChannel hChannel, const uint8_t *buf)
{
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
    // sync remote session id to local
    uint32_t unOld = hChannel->channelId;
    hChannel->channelId = ntohl(hShake->channelId);
    AdminChannel(OP_UPDATE, unOld, hChannel);
    WRITE_LOG(LOG_DEBUG, "Client channel handshake finished, use connectkey:%s",
              Hdc::MaskString(connectKey).c_str());
    // send config
    // channel handshake step2
    if (memset_s(hShake->connectKey, sizeof(hShake->connectKey), 0, sizeof(hShake->connectKey)) != EOK
        || memcpy_s(hShake->connectKey, sizeof(hShake->connectKey), connectKey.c_str(), connectKey.size()) != EOK) {
        hChannel->availTailIndex = 0;
        WRITE_LOG(LOG_DEBUG, "Channel Hello failed");
        return ERR_BUF_COPY;
    }
#ifdef HDC_VERSION_CHECK
    // add check version
    if (!isCheckVersionCmd) { // do not check version cause user want to get server version
        string clientVer = Base::GetVersion() + HDC_MSG_HASH;
        string serverVer = CharPointToString(hShake->version, sizeof(hShake->version));
        if (clientVer != serverVer) {
            if (serverVer.size() >= Base::GetVersion().size()) {
                serverVer = serverVer.substr(0, Base::GetVersion().size());
            }
            WRITE_LOG(LOG_FATAL, "Client version:%s, server version:%s", clientVer.c_str(), serverVer.c_str());
            hChannel->availTailIndex = 0;
            return ERR_CHECK_VERSION;
        }
    }
    Send(hChannel->channelId, reinterpret_cast<uint8_t *>(hShake), sizeof(ChannelHandShake));
#else
        // do not send version message if check feature disable
    Send(hChannel->channelId, reinterpret_cast<uint8_t *>(hShake), offsetof(struct ChannelHandShake, version));
#endif
    hChannel->handshakeOK = true;
#ifdef HDC_CHANNEL_KEEP_ALIVE
    // Evaluation method, non long-term support
    Send(hChannel->channelId,
         reinterpret_cast<uint8_t *>(const_cast<char*>(CMDSTR_INNER_ENABLE_KEEPALIVE.c_str())),
         CMDSTR_INNER_ENABLE_KEEPALIVE.size());
#endif
    return RET_SUCCESS;
}

// read serverForClient(server)TCP data
int HdcClient::ReadChannel(HChannel hChannel, uint8_t *buf, const int bytesIO)
{
    if (!hChannel->handshakeOK) {
        return PreHandshake(hChannel, buf);
    }
#ifdef UNIT_TEST
    // Do not output to console when the unit test
    return 0;
#endif
    WRITE_LOG(LOG_DEBUG, "Client ReadChannel :%d", bytesIO);

    uint16_t cmd = 0;
    bool bOffset = false;
    if (bytesIO >= static_cast<int>(sizeof(uint16_t))) {
        cmd = *reinterpret_cast<uint16_t *>(buf);
        bOffset = IsOffset(cmd);
    }
    if (cmd == CMD_CHECK_SERVER && isCheckVersionCmd) {
        WRITE_LOG(LOG_DEBUG, "recieve CMD_CHECK_VERSION command");
        string version(reinterpret_cast<char *>(buf + sizeof(uint16_t)), bytesIO - sizeof(uint16_t));
        fprintf(stdout, "Client version:%s, server version:%s\n", Base::GetVersion().c_str(), version.c_str());
        fflush(stdout);
        return 0;
    }
    if (hChannel->remote > RemoteType::REMOTE_NONE && bOffset) {
        // file command
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
        // app command
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
        return 0;
    }

    string s(reinterpret_cast<char *>(buf), bytesIO);
    if (WaitFor(s)) {
        return 0;
    }
    s = ListTargetsAll(s);
    if (g_show) {
#ifdef _WIN32
        fprintf(stdout, "%s", s.c_str());
        fflush(stdout);
#else
        constexpr int len = 512;
        int size = bytesIO / len;
        int left = bytesIO % len;
        for (int i = 0; i <= size; i++) {
            int cnt = len;
            const char *p = reinterpret_cast<char *>(buf) + i * cnt;
            if (i == size) {
                cnt = left;
            }
            fprintf(stdout, "%.*s", cnt, p);
            fflush(stdout);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
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
    SystemDepend::GetDevItem(SYS_PARAM_ENTERPRISE_HDC_DISABLE.c_str(), out);
    if (out.empty() || out == "false") {
        return false;
    }
    return true;
}
#endif
}  // namespace Hdc
