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
#ifndef HDC_DAEMON_H
#define HDC_DAEMON_H
#include "daemon_common.h"

namespace Hdc {
enum UserPermit {
    REFUSE = 0,
    ALLOWONCE = 1,
    ALLOWFORVER = 2,
};
struct HdcDaemonAuthInfo {
    HdcSessionBase::AuthType authtype;
    string token;
    string pubkey;
    string authmsg;
};
class HdcDaemon : public HdcSessionBase {
public:
#ifdef USE_CONFIG_UV_THREADS
    HdcDaemon(bool serverOrDaemonIn, size_t uvThreadSize = SIZE_THREAD_POOL);
#else
    HdcDaemon(bool serverOrDaemonIn);
#endif
    virtual ~HdcDaemon();
#ifdef HDC_SUPPORT_UART
#ifdef HDC_EMULATOR
    void InitMod(bool bEnableTCP, bool bEnableUSB, bool bEnableBridge, bool bEnableUART);
#endif
    void InitMod(bool bEnableTCP, bool bEnableUSB, bool bEnableUART);
#else
#ifdef HDC_EMULATOR
    void InitMod(bool bEnableTCP, bool bEnableUSB, bool bEnableBridge);
#endif
    void InitMod(bool bEnableTCP, bool bEnableUSB);
#endif
    bool FetchCommand(HSession hSession, const uint32_t channelId, const uint16_t command, uint8_t *payload,
                      const int payloadSize) override;
    bool ServerCommand(const uint32_t sessionId, const uint32_t channelId, const uint16_t command, uint8_t *bufPtr,
                       const int size) override;
    void ClearKnownHosts();
    void *clsTCPServ;
    void *clsUSBServ;
#ifdef HDC_EMULATOR
    void *clsBridgeServ;
#endif
#ifdef HDC_SUPPORT_UART
    void *clsUARTServ;
#endif
    void *clsJdwp;

private:
    bool RemoveInstanceTask(const uint8_t op, HTaskInfo hTask) override;
    bool RedirectToTask(HTaskInfo hTaskInfo, HSession hSession, const uint32_t channelId, const uint16_t command,
                        uint8_t *payload, const int payloadSize) override;
    void JdwpNewFileDescriptor(const uint8_t *buf, const int bytesIO) override;
    static bool CheckControl(const uint16_t command);
    static bool IsExpectedParam(const std::string& param, const std::string& expect);
    bool HandDaemonAuth(HSession hSession, const uint32_t channelId, SessionHandShake &handshake);
    bool GetHostPubkeyInfo(const string& buf, string& hostname, string& pubkey);
    bool AlreadyInKnownHosts(const string& key);
    void UpdateKnownHosts(const string& key);
    void ClearInstanceResource() override;
    void DaemonSessionHandshakeInit(HSession &hSession, SessionHandShake &handshake);
    bool DaemonSessionHandshake(HSession hSession, const uint32_t channelId, uint8_t *payload, int payloadSize);
    void TryStopInstance();
    UserPermit PostUIConfirm(string hostname);
    bool ShowPermitDialog();
    bool HandDaemonAuthInit(HSession hSession, const uint32_t channelId, SessionHandShake &handshake);
    bool HandDaemonAuthPubkey(HSession hSession, const uint32_t channelId, SessionHandShake &handshake);
    bool HandDaemonAuthSignature(HSession hSession, const uint32_t channelId, SessionHandShake &handshake);
// deprecated, remove later
#ifdef HDC_SUPPORT_FLASHD
// null
#else
    void NotifyInstanceSessionFree(HSession hSession, bool freeOrClear) override;
#endif

    bool HandDaemonAuthBypass(void);
    void SendAuthSignMsg(SessionHandShake &handshake,
            uint32_t channelId, uint32_t sessionid, string pubkey, string token);
    void SendAuthOkMsg(SessionHandShake &handshake, uint32_t channelid, uint32_t sessionid, string msg);
    void AuthRejectLowClient(SessionHandShake &handshake, uint32_t channelid, uint32_t sessionid);
    void EchoHandshakeMsg(SessionHandShake &handshake, uint32_t channelid, uint32_t sessionid, string msg);
    bool AuthVerify(HSession hSession, string encryptToken);
    void InitSessionAuthInfo(uint32_t sessionid, string token);
    void UpdateSessionAuthOk(uint32_t sessionid);
    void UpdateSessionAuthmsg(uint32_t sessionid, string authmsg);
    void UpdateSessionAuthPubkey(uint32_t sessionid, string pubkey);
    void DeleteSessionAuthStatus(uint32_t sessionid);
    AuthType GetSessionAuthStatus(uint32_t sessionid);
    string GetSessionAuthmsg(uint32_t sessionid);
    string GetSessionAuthToken(uint32_t sessionid);
    string GetSessionAuthPubkey(uint32_t sessionid);
    std::map<uint32_t, HdcDaemonAuthInfo> mapAuthStatus;
    std::mutex mapAuthStatusMutex;
    bool enableSecure;
};
}  // namespace Hdc
#endif
