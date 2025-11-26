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
#ifndef HDC_CLIENT_H
#define HDC_CLIENT_H
#include "host_common.h"

extern std::map<std::string, std::string> g_lists;
extern bool g_show;

namespace Hdc {
// Avoiding Circular dependency
class HdcHostApp;

class HdcClient : public HdcChannelBase {
public:
    HdcClient(const bool serverOrClient, const string &addrString, uv_loop_t *loopMainIn, bool checkVersion = false);
    virtual ~HdcClient();
    int Initial(const string &connectKeyIn);
    int ExecuteCommand(const string &commandIn);
    int CtrlServiceWork(const char *commandIn);
    bool ChannelCtrlServer(const string &cmd, string &connectKey);

protected:
private:
    static void DoCtrlServiceWork(uv_check_t *handle);
    static void Connect(uv_connect_t *connection, int status);
    static void AllocStdbuf(uv_handle_t *handle, size_t sizeWanted, uv_buf_t *buf);
    static void ReadStd(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
    static void CommandWorker(uv_timer_t *handle);
    static void RetryTcpConnectWorker(uv_timer_t *handle);
#ifdef __OHOS__
    static void RetryUdsConnectWorker(uv_timer_t *handle);
    static void ConnectUds(uv_connect_t *connection, int status);
    int ConnectUdsServerForClient();
    bool IsNeedInterceptCommand();
#endif
    int ConnectServerForClient(const char *ip, uint16_t port);
    int ReadChannel(HChannel hChannel, uint8_t *buf, const int bytesIO) override;
    int PreHandshake(HChannel hChannel, const uint8_t *buf);
    string AutoConnectKey(string &doCommand, const string &preConnectKey) const;
    uint32_t GetLastPID();
    bool StartServer(const string &cmd);
    bool KillServer(const string &cmd);
    void BindLocalStd();
    void BindLocalStd(HChannel hChannel);
    void ModifyTty(bool setOrRestore, uv_tty_t *tty);
    void NotifyInstanceChannelFree(HChannel hChannel) override;
    bool IsOffset(uint16_t cmd);
    HTaskInfo GetRemoteTaskInfo(HChannel hChannel);
    bool WaitFor(const string &str);
    string ListTargetsAll(const string &str);
    void UpdateList(const string &str);
    bool KillMethodByUv(bool isStart);
    std::string CharPointToString(const char *buf, const int size);

#ifdef _WIN32
    static string GetHilogPath();
    void RunCommandWin32(const string& cmd);
    bool CreatePipePair(HANDLE *hParentRead, HANDLE *hSubWrite, HANDLE *hSubRead, HANDLE *hParentWrite,
        SECURITY_ATTRIBUTES *sa);
    bool CreateChildProcess(HANDLE hSubWrite, HANDLE hSubRead, PROCESS_INFORMATION *pi, const string& cmd);
#else
    static void RunCommand(const string& cmd);
#endif
    void RunExecuteCommand(const string& cmd);

#ifndef _WIN32
    termios terminalState;
#endif
    string connectKey;
    string command;
    uint16_t debugRetryCount;
    bool bShellInteractive = false;
    uv_timer_t waitTimeDoCmd;
    uv_check_t ctrlServerWork;
    HChannel channel;
    std::unique_ptr<HdcFile> fileTask;
    std::unique_ptr<HdcHostApp> appTask;
    bool isCheckVersionCmd = false;
    bool isIpV4 = true;
    struct sockaddr_in destv4;
    struct sockaddr_in6 dest;
    uv_timer_t retryTcpConnTimer;
    uint16_t tcpConnectRetryCount = 0;
#ifdef __OHOS__
    string serverAddress;
    uv_timer_t retryUdsConnTimer;
    uint16_t udsConnectRetryCount = 0;
#endif
};
}  // namespace Hdc
#endif
