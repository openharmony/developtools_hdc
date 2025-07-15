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
#ifndef HDC_SERVERFORCLIENT_H
#define HDC_SERVERFORCLIENT_H
#include "host_common.h"
#include "translate.h"

namespace Hdc {
class HdcServerForClient : public HdcChannelBase {
public:
    HdcServerForClient(const bool serverOrClient, const string &addrString, void *pClsServer, uv_loop_t *loopMainIn);
    virtual ~HdcServerForClient();
    int Initial();
    void EchoClient(HChannel hChannel, MessageLevel level, const char *msg, ...);
    void EchoClientRaw(const HChannel hChannel, uint8_t *payload, const int payloadSize);
    void SendCommandToClient(const HChannel hChannel, const uint16_t commandFlag, uint8_t *payload,
                             const int payloadSize);
    uint16_t GetTCPListenPort();
    void Stop();

protected:
private:
    static void AcceptClient(uv_stream_t *server, int status);
    bool SetTCPListen();
    int ReadChannel(HChannel hChannel, uint8_t *bufPtr, const int bytesIO) override;
    void ReportServerVersion(HChannel hChannel);
    bool DoCommand(HChannel hChannel, void *formatCommandInput, HDaemonInfo &hdi);
    void OrderFindTargets(HChannel hChannel);
    bool NewConnectTry(void *ptrServer, HChannel hChannel, const string &connectKey, bool isCheck = false);
    static void OrderConnecTargetResult(uv_timer_t *req);
    bool SendToDaemon(HChannel hChannel, const uint16_t commandFlag, uint8_t *bufPtr, const int bufSize);
    int BindChannelToSession(HChannel hChannel, uint8_t *bufPtr, const int bytesIO);
    bool CheckAutoFillTarget(HChannel hChannel);
    bool CommandRemoveSession(HChannel hChannel, const char *connectKey);
    bool CommandRemoveForward(const string &forwardKey);
    bool RemoveFportkey(const string &forwardKey);
    bool DoCommandLocal(HChannel hChannel, void *formatCommandInput);
    bool DoCommandRemote(HChannel hChannel, void *formatCommandInput);
    void GetTargetList(HChannel hChannel, void *formatCommandInput);
    bool GetAnyTarget(HChannel hChannel);
    bool WaitForAny(HChannel hChannel);
    bool RemoveForward(HChannel hChannel, const char *parameterString);
    bool TaskCommand(HChannel hChannel, void *formatCommandInput);
    void HandleRemote(HChannel hChannel, string &parameters, RemoteType flag);
    int ChannelHandShake(HChannel hChannel, uint8_t *bufPtr, const int bytesIO);
    bool ChannelSendSessionCtrlMsg(vector<uint8_t> &ctrlMsg, uint32_t sessionId) override;
    HSession FindAliveSession(uint32_t sessionId);
    HSession FindAliveSessionFromDaemonMap(const HChannel hChannel);
    string GetErrorString(uint32_t errorCode);
    void PrintLastError(HChannel HChannel);
    bool CommandMatchDaemonFeature(uint16_t cmdFlag, const HDaemonInfo &hdi);
#ifdef __OHOS__
    bool IsServerTransfer(HChannel hChannel, uint16_t cmdFlag, string &parameters);
#endif

    uv_tcp_t tcpListen;
    void *clsServer;
    const std::set<uint16_t> FEATURE_CHECK_SET = {
        CMD_UNITY_EXECUTE_EX,
    }; // feature cmdFlag
    const std::unordered_map<uint32_t, std::string> ErrorStringEnglish = {
#ifdef _WIN32
        {0x000005, "Failed to load the authorization pub key!\r\n"
            "Please check the public key directory:[%USERPROFILE%\\.harmony]\r\n"
            "Alternatively, delete the public key directory and re-run the command.\r\n"
            "Delete command: \"del %USERPROFILE%\\.harmony\""},
#else
        {0x000005, "Failed to load the authorization pub key!\r\n"
            "Please check the public key directory:[~/.harmony]\r\n"
            "Alternatively, delete the public key directory and re-run the command.\r\n"
            "Delete command: \"rm -r ~/.harmony\""},
#endif
        {0x002103, "Failed to start the HDC server process!\r\n"
            "Please check the HDC server process port is occupied or used as an exception port.\r\n"
            "Alternatively, change the OHOS_HDC_SERVER_PORT environment variable and re-run the command."},
        {0xFFFFFF, "Unknown error"},
    };
};
}  // namespace Hdc
#endif
