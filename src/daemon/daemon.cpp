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
#include "daemon.h"
#ifndef TEST_HASH
#include "hdc_hash_gen.h"
#endif
#include "serial_struct.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>

namespace Hdc {
#ifdef USE_CONFIG_UV_THREADS
HdcDaemon::HdcDaemon(bool serverOrDaemonIn, size_t uvThreadSize)
    : HdcSessionBase(serverOrDaemonIn, uvThreadSize)
#else
HdcDaemon::HdcDaemon(bool serverOrDaemonIn)
    : HdcSessionBase(serverOrDaemonIn, -1)
#endif
{
    clsTCPServ = nullptr;
    clsUSBServ = nullptr;
#ifdef HDC_EMULATOR
    clsBridgeServ = nullptr;
#endif
#ifdef HDC_SUPPORT_UART
    clsUARTServ = nullptr;
#endif
    clsJdwp = nullptr;
    enableSecure = false;
}

HdcDaemon::~HdcDaemon()
{
    WRITE_LOG(LOG_DEBUG, "~HdcDaemon");
}

void HdcDaemon::ClearInstanceResource()
{
    TryStopInstance();
    Base::TryCloseLoop(&loopMain, "HdcDaemon::~HdcDaemon");
    if (clsTCPServ) {
        delete (HdcDaemonTCP *)clsTCPServ;
        clsTCPServ = nullptr;
    }
    if (clsUSBServ) {
        delete (HdcDaemonUSB *)clsUSBServ;
        clsUSBServ = nullptr;
    }
#ifdef HDC_EMULATOR
    if (clsBridgeServ) {
        delete (HdcDaemonBridge *)clsBridgeServ;
    }
#endif
#ifdef HDC_SUPPORT_UART
    if (clsUARTServ) {
        delete (HdcDaemonUART *)clsUARTServ;
    }
    clsUARTServ = nullptr;
#endif
    if (clsJdwp) {
        delete (HdcJdwp *)clsJdwp;
        clsJdwp = nullptr;
    }
    WRITE_LOG(LOG_DEBUG, "~HdcDaemon finish");
}

void HdcDaemon::TryStopInstance()
{
    ClearSessions();
    if (clsTCPServ) {
        WRITE_LOG(LOG_DEBUG, "Stop TCP");
        ((HdcDaemonTCP *)clsTCPServ)->Stop();
    }
    if (clsUSBServ) {
        WRITE_LOG(LOG_DEBUG, "Stop USB");
        ((HdcDaemonUSB *)clsUSBServ)->Stop();
    }
#ifdef HDC_EMULATOR
    if (clsBridgeServ) {
        WRITE_LOG(LOG_DEBUG, "Stop Bridge");
        ((HdcDaemonBridge *)clsBridgeServ)->Stop();
    }
#endif
#ifdef HDC_SUPPORT_UART
    if (clsUARTServ) {
        WRITE_LOG(LOG_DEBUG, "Stop UART");
        ((HdcDaemonUART *)clsUARTServ)->Stop();
    }
#endif
    ((HdcJdwp *)clsJdwp)->Stop();
    // workaround temply remove MainLoop instance clear
    ReMainLoopForInstanceClear();
    WRITE_LOG(LOG_DEBUG, "Stop loopmain");
}

#ifdef HDC_SUPPORT_UART
void HdcDaemon::InitMod(bool bEnableTCP, bool bEnableUSB, [[maybe_unused]] bool bEnableUART)
#else
void HdcDaemon::InitMod(bool bEnableTCP, bool bEnableUSB)
#endif
{
    WRITE_LOG(LOG_DEBUG, "HdcDaemon InitMod");
#ifdef HDC_SUPPORT_UART
    WRITE_LOG(LOG_DEBUG, "bEnableTCP:%d,bEnableUSB:%d", bEnableTCP, bEnableUSB);
#endif
    if (bEnableTCP) {
        // tcp
        clsTCPServ = new(std::nothrow) HdcDaemonTCP(false, this);
        if (clsTCPServ == nullptr) {
            WRITE_LOG(LOG_FATAL, "InitMod new clsTCPServ failed");
            return;
        }
        ((HdcDaemonTCP *)clsTCPServ)->Initial();
    }
    if (bEnableUSB) {
        // usb
        clsUSBServ = new(std::nothrow) HdcDaemonUSB(false, this);
        if (clsUSBServ == nullptr) {
            WRITE_LOG(LOG_FATAL, "InitMod new clsUSBServ failed");
            return;
        }
        ((HdcDaemonUSB *)clsUSBServ)->Initial();
    }
#ifdef HDC_SUPPORT_UART
    WRITE_LOG(LOG_DEBUG, "bEnableUART:%d", bEnableUART);
    if (bEnableUART) {
        // UART
        clsUARTServ = new(std::nothrow) HdcDaemonUART(*this);
        if (clsUARTServ == nullptr) {
            WRITE_LOG(LOG_FATAL, "InitMod new clsUARTServ failed");
            return;
        }
        ((HdcDaemonUART *)clsUARTServ)->Initial();
    }
#endif
    clsJdwp = new(std::nothrow) HdcJdwp(&loopMain);
    if (clsJdwp == nullptr) {
        WRITE_LOG(LOG_FATAL, "InitMod new clsJdwp failed");
        return;
    }
    ((HdcJdwp *)clsJdwp)->Initial();
    // enable security
    string secure;
    SystemDepend::GetDevItem("const.hdc.secure", secure);
    string authbypass;
    SystemDepend::GetDevItem("persist.hdc.auth_bypass", authbypass);
#ifndef HDC_EMULATOR
    enableSecure = ((Base::Trim(secure) == "1") && (Base::Trim(authbypass) != "1"));
#endif
}

#ifdef HDC_EMULATOR
#ifdef HDC_SUPPORT_UART
void HdcDaemon::InitMod(bool bEnableTCP, bool bEnableUSB, bool bEnableBridge, [[maybe_unused]] bool bEnableUART)
{
    InitMod(bEnableTCP, bEnableUSB, bEnableUART);
#else
void HdcDaemon::InitMod(bool bEnableTCP, bool bEnableUSB, bool bEnableBridge)
{
    InitMod(bEnableTCP, bEnableUSB);
#endif
    if (bEnableBridge) {
        clsBridgeServ = new(std::nothrow) HdcDaemonBridge(false, this);
        if (clsBridgeServ == nullptr) {
            WRITE_LOG(LOG_FATAL, "InitMod new clsBridgeServ failed");
            return;
        }
        ((HdcDaemonBridge *)clsBridgeServ)->Initial();
    }
}
#endif

// clang-format off
bool HdcDaemon::RedirectToTask(HTaskInfo hTaskInfo, HSession hSession, const uint32_t channelId,
                               const uint16_t command, uint8_t *payload, const int payloadSize)
{
    StartTraceScope("HdcDaemon::RedirectToTask");
    bool ret = true;
    hTaskInfo->ownerSessionClass = this;
    switch (command) {
        case CMD_UNITY_EXECUTE:
        case CMD_UNITY_REMOUNT:
        case CMD_UNITY_REBOOT:
        case CMD_UNITY_RUNMODE:
        case CMD_UNITY_HILOG:
        case CMD_UNITY_ROOTRUN:
        case CMD_UNITY_TERMINATE:
        case CMD_UNITY_BUGREPORT_INIT:
        case CMD_JDWP_LIST:
        case CMD_JDWP_TRACK:
            ret = TaskCommandDispatch<HdcDaemonUnity>(hTaskInfo, TYPE_UNITY, command, payload, payloadSize);
            break;
        case CMD_SHELL_INIT:
        case CMD_SHELL_DATA:
            ret = TaskCommandDispatch<HdcShell>(hTaskInfo, TYPE_SHELL, command, payload, payloadSize);
            break;
        case CMD_FILE_CHECK:
        case CMD_FILE_DATA:
        case CMD_FILE_FINISH:
        case CMD_FILE_INIT:
        case CMD_FILE_BEGIN:
        case CMD_FILE_MODE:
        case CMD_DIR_MODE:
            ret = TaskCommandDispatch<HdcFile>(hTaskInfo, TASK_FILE, command, payload, payloadSize);
            break;
        // One-way function, so fewer options
        case CMD_APP_CHECK:
        case CMD_APP_DATA:
        case CMD_APP_UNINSTALL:
            ret = TaskCommandDispatch<HdcDaemonApp>(hTaskInfo, TASK_APP, command, payload, payloadSize);
            break;
        case CMD_FORWARD_INIT:
        case CMD_FORWARD_CHECK:
        case CMD_FORWARD_ACTIVE_MASTER:
        case CMD_FORWARD_ACTIVE_SLAVE:
        case CMD_FORWARD_DATA:
        case CMD_FORWARD_FREE_CONTEXT:
        case CMD_FORWARD_CHECK_RESULT:
            ret = TaskCommandDispatch<HdcDaemonForward>(hTaskInfo, TASK_FORWARD, command, payload, payloadSize);
            break;
        default:
        // ignore unknown command
            break;
    }
    return ret;
}
// clang-format on

bool HdcDaemon::ShowPermitDialog()
{
    pid_t pid;
    int fds[2];
    pipe(fds);

    if ((pid = fork()) == -1) {
        WRITE_LOG(LOG_FATAL, "fork failed %s", strerror(errno));
        return false;
    }
    if (pid == 0) {
        Base::DeInitProcess();
        // close the child read channel
        close(fds[0]);
        // redirect the child write channel
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);

        setsid();
        setpgid(pid, pid);

        int ret = execl("/system/bin/hdcd_user_permit", "hdcd_user_permit", NULL);
        // if execl failed need return false
        WRITE_LOG(LOG_FATAL, "start user_permit failed %d: %s", ret, strerror(errno));
        return false;
    } else {
            Base::CloseFd(fds[1]);
            waitpid(pid, nullptr, 0);
            char buf[1024] = { 0 };
            int nbytes = read(fds[0], buf, sizeof(buf));
            WRITE_LOG(LOG_FATAL, "user_permit put %d bytes: %s", nbytes, buf);
            close(fds[0]);
    }

    return true;
}

UserPermit HdcDaemon::PostUIConfirm(string hostname)
{
    // clear result first
    if (!SystemDepend::SetDevItem("persist.hdc.daemon.auth_result", "auth_result_none")) {
        WRITE_LOG(LOG_FATAL, "debug auth result failed, so refuse this connect");
        return REFUSE;
    }

    // then write para for setting
    if (!SystemDepend::SetDevItem("persist.hdc.client.hostname", hostname.c_str())) {
        WRITE_LOG(LOG_FATAL, "set param(%s) failed", hostname.c_str());
        return REFUSE;
    }
    if (!ShowPermitDialog()) {
        WRITE_LOG(LOG_FATAL, "show dialog failed, so refuse this connect.");
        return REFUSE;
    }

    string authResult;
    if (!SystemDepend::GetDevItem("persist.hdc.daemon.auth_result", authResult)) {
        WRITE_LOG(LOG_FATAL, "user refuse [%s] this developer [%s]", authResult.c_str(), hostname.c_str());
        return REFUSE;
    }
    WRITE_LOG(LOG_FATAL, "user permit_result [%s] for this developer [%s]", authResult.c_str(), hostname.c_str());
    string prifix = "auth_result:";
    string result = authResult.substr(prifix.length());
    if (result == "1") {
        return ALLOWONCE;
    }
    if (result == "2") {
        return ALLOWFORVER;
    }
    return REFUSE;
}

bool HdcDaemon::GetHostPubkeyInfo(const string& buf, string& hostname, string& pubkey)
{
    // "\f" asicc is 0x0C
    char separator = '\x0C';

    hostname = buf.substr(0, buf.find(separator));
    pubkey = buf.substr(buf.find(separator) + 1);
    WRITE_LOG(LOG_INFO, "hostname is [%s], pubkey is [%s]", hostname.c_str(), pubkey.c_str());

    return (!hostname.empty() && !pubkey.empty());
}

void HdcDaemon::ClearKnownHosts()
{
    char const *keyfile = "/data/service/el0/hdc/hdc_keys";

    if (!enableSecure || HandDaemonAuthBypass()) {
        WRITE_LOG(LOG_INFO, "not enable secure, noneed clear keyfile");
        return;
    }

    string authcancel;
    if (!SystemDepend::GetDevItem("persist.hdc.daemon.auth_cancel", authcancel)) {
        WRITE_LOG(LOG_FATAL, "get param auth_cancel failed");
        return;
    }
    if (authcancel != "true") {
        WRITE_LOG(LOG_FATAL, "param auth_cancel is not true: %s", authcancel.c_str());
        return;
    }
    if (!SystemDepend::SetDevItem("persist.hdc.daemon.auth_cancel", "false")) {
        WRITE_LOG(LOG_FATAL, "set param auth_cancel failed");
    }

    std::ofstream keyofs(keyfile, std::ios::out | std::ios::trunc);
    if (!keyofs.is_open()) {
        WRITE_LOG(LOG_FATAL, "open keyfile %s error", keyfile);
        return;
    }

    keyofs.flush();
    keyofs.close();

    WRITE_LOG(LOG_FATAL, "clear keyfile %s over", keyfile);

    return;
}

void HdcDaemon::UpdateKnownHosts(const string& key)
{
    char const *keyfile = "/data/service/el0/hdc/hdc_keys";

    std::ofstream keyofs(keyfile, std::ios::app);
    if (!keyofs.is_open()) {
        WRITE_LOG(LOG_FATAL, "open keyfile %s error", keyfile);
        return;
    }

    string keytmp = key + "\n";
    keyofs.write(keytmp.c_str(), keytmp.length());
    keyofs.flush();
    keyofs.close();

    WRITE_LOG(LOG_FATAL, "save new key [%s] into keyfile %s over", key.c_str(), keyfile);

    return;
}

bool HdcDaemon::AlreadyInKnownHosts(const string& key)
{
    char const *keyfile = "/data/service/el0/hdc/hdc_keys";

    std::ifstream keyifs(keyfile);
    if (!keyifs.is_open()) {
        WRITE_LOG(LOG_FATAL, "open keyfile %s error", keyfile);
        return false;
    }

    std::string keys((std::istreambuf_iterator<char>(keyifs)), std::istreambuf_iterator<char>());
    if (keys.find(key) != string::npos) {
        keyifs.close();
        return true;
    }

    WRITE_LOG(LOG_FATAL, "key [%s] not in keyfile %s", key.c_str(), keyfile);

    keyifs.close();
    return false;
}

bool HdcDaemon::HandDaemonAuthInit(HSession hSession, const uint32_t channelId, SessionHandShake &handshake)
{
    hSession->tokenRSA = Base::GetRandomString(SHA_DIGEST_LENGTH);
    handshake.authType = AUTH_PUBLICKEY;
    handshake.buf = hSession->tokenRSA;
    string bufString = SerialStruct::SerializeToString(handshake);
    Send(hSession->sessionId, channelId, CMD_KERNEL_HANDSHAKE,
            reinterpret_cast<uint8_t *>(const_cast<char *>(bufString.c_str())),
            bufString.size());

    InitSessionAuthInfo(hSession->sessionId, hSession->tokenRSA);
    return true;
}

bool HdcDaemon::HandDaemonAuthPubkey(HSession hSession, const uint32_t channelId, SessionHandShake &handshake)
{
    bool ret = false;
    string hostname, pubkey;

    do {
        if (!GetHostPubkeyInfo(handshake.buf, hostname, pubkey)) {
            WRITE_LOG(LOG_FATAL, "get pubkey failed for %u", hSession->sessionId);
            break;
        }
        if (AlreadyInKnownHosts(pubkey)) {
            ret = true;
            break;
        }

        string confirmmsg = "[E000002]:The device unauthorized.\n"\
                             "This server's public key is not set.\n"\
                             "Please check for a confirmation dialog on your device.\n"\
                             "Otherwise try 'hdc kill' if that seems wrong.";
        std::thread notifymsg(&HdcDaemon::EchoHandshakeMsg, this,
                    std::ref(handshake), channelId, hSession->sessionId, confirmmsg);
        notifymsg.detach();

        UserPermit permit = PostUIConfirm(hostname);
        if (permit == ALLOWONCE) {
            WRITE_LOG(LOG_FATAL, "user allow onece for %u", hSession->sessionId);
            ret = true;
        } else if (permit == ALLOWFORVER) {
            WRITE_LOG(LOG_FATAL, "user allow forever for %u", hSession->sessionId);
            UpdateKnownHosts(pubkey);
            ret = true;
        } else {
            WRITE_LOG(LOG_FATAL, "user refuse for %u", hSession->sessionId);
            ret = false;
        }
    } while (0);

    if (ret) {
        SendAuthSignMsg(handshake, channelId, hSession->sessionId, pubkey, hSession->tokenRSA);
    } else {
        string notifymsg = "[E000003]:The device unauthorized.\n"\
                            "The user denied the access for the device.\n"\
                             "Please execute 'hdc kill' and redo your command, "\
                             "then check for a confirmation dialog on your device.";
        EchoHandshakeMsg(handshake, channelId, hSession->sessionId, notifymsg);
    }
    return true;
}

bool HdcDaemon::AuthVerify(HSession hSession, string encryptToken)
{
    string token = GetSessionAuthToken(hSession->sessionId);
    string pubkey = GetSessionAuthPubkey(hSession->sessionId);
    const unsigned char *pubkeyp = reinterpret_cast<const unsigned char *>(pubkey.c_str());
    const unsigned char *tokenp = reinterpret_cast<const unsigned char *>(encryptToken.c_str());
    unsigned char tokenDecode[1024] = { 0 };
    unsigned char decryptToken[BUF_SIZE_DEFAULT2] = { 0 };
    BIO *bio = nullptr;
    RSA *rsa = nullptr;
    bool verifyret = false;

    do {
        int tbytes = EVP_DecodeBlock(tokenDecode, tokenp, encryptToken.length());
        if (tbytes <= 0) {
            WRITE_LOG(LOG_FATAL, "base64 decode pubkey failed");
            break;
        }

        bio = BIO_new(BIO_s_mem());
        if (bio == nullptr) {
            WRITE_LOG(LOG_FATAL, "bio failed for session %u", hSession->sessionId);
            break;
        }
        int wbytes = BIO_write(bio, pubkeyp, pubkey.length());
        if (wbytes <= 0) {
            WRITE_LOG(LOG_FATAL, "bio write failed %d for session %u", wbytes, hSession->sessionId);
            break;
        }
        rsa = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
        if (rsa == nullptr) {
            WRITE_LOG(LOG_FATAL, "rsa failed for session %u", hSession->sessionId);
            break;
        }
        int bytes = RSA_public_decrypt(tbytes, tokenDecode, decryptToken, rsa, RSA_PKCS1_PADDING);
        if (bytes < 0) {
            WRITE_LOG(LOG_FATAL, "decrypt failed(%lu) for session %u", ERR_get_error(), hSession->sessionId);
            break;
        }
        string sdecryptToken(reinterpret_cast<const char *>(decryptToken), bytes);
        if (sdecryptToken != token) {
            WRITE_LOG(LOG_FATAL, "auth failed(%lu) for session %u)",
                    ERR_get_error(), hSession->sessionId);
            break;
        }

        WRITE_LOG(LOG_FATAL, "auth success for session %u", hSession->sessionId);
        verifyret = true;
    } while (0);

    if (bio) {
        BIO_free(bio);
    }
    if (rsa) {
        RSA_free(rsa);
    }

    return verifyret;
}

bool HdcDaemon::HandDaemonAuthSignature(HSession hSession, const uint32_t channelId, SessionHandShake &handshake)
{
    // When Host is first connected to the device, the signature authentication is inevitable, and the
    // certificate verification must be triggered.
    //
    // When the certificate is verified, the client sends a public key to the device, triggered the system UI
    // jump out dialog, and click the system, the system will store the Host public key certificate in the
    // device locally, and the signature authentication will be correct when the subsequent connection is
    // connected.
    if (!AuthVerify(hSession, handshake.buf)) {
        WRITE_LOG(LOG_FATAL, "auth failed for %u", hSession->sessionId);
        // Next auth
        EchoHandshakeMsg(handshake, channelId, hSession->sessionId, "[E000010]:Auth failed, cannt login the device.");
        return true;
    }

    UpdateSessionAuthOk(hSession->sessionId);
    SendAuthOkMsg(handshake, channelId, hSession->sessionId);
    return true;
}

bool HdcDaemon::HandDaemonAuthBypass(void)
{
    // persist.hdc.auth_bypass 1 is bypass orelse(0 or not set) not bypass
    string authbypass;
    SystemDepend::GetDevItem("persist.hdc.auth_bypass", authbypass);
    return Base::Trim(authbypass) == "1";
}

bool HdcDaemon::HandDaemonAuth(HSession hSession, const uint32_t channelId, SessionHandShake &handshake)
{
    if (!enableSecure) {
        WRITE_LOG(LOG_INFO, "not enable secure, allow access for %u", hSession->sessionId);
        UpdateSessionAuthOk(hSession->sessionId);
        SendAuthOkMsg(handshake, channelId, hSession->sessionId);
        return true;
    } else if (HandDaemonAuthBypass()) {
        WRITE_LOG(LOG_INFO, "auth bypass, allow access for %u", hSession->sessionId);
        UpdateSessionAuthOk(hSession->sessionId);
        SendAuthOkMsg(handshake, channelId, hSession->sessionId);
        return true;
    } else if (handshake.version < "Ver: 3.0.0b") {
        WRITE_LOG(LOG_INFO, "session %u client version %s is too low %u authType %d",
                    hSession->sessionId, handshake.version.c_str(), handshake.authType);
        AuthRejectLowClient(handshake, channelId, hSession->sessionId);
        return true;
    } else if (GetSessionAuthStatus(hSession->sessionId) == AUTH_OK) {
        WRITE_LOG(LOG_INFO, "session %u already auth ok", hSession->sessionId);
        return true;
    }

    if (handshake.authType == AUTH_NONE) {
        return HandDaemonAuthInit(hSession, channelId, handshake);
    } else if (handshake.authType == AUTH_PUBLICKEY) {
        return HandDaemonAuthPubkey(hSession, channelId, handshake);
    } else if (handshake.authType == AUTH_SIGNATURE) {
        return HandDaemonAuthSignature(hSession, channelId, handshake);
    } else {
        WRITE_LOG(LOG_FATAL, "invalid auth state %d for session %u", handshake.authType, hSession->sessionId);
        return false;
    }
}

void HdcDaemon::DaemonSessionHandshakeInit(HSession &hSession, SessionHandShake &handshake)
{
    // daemon handshake 1st packet
    uint32_t unOld = hSession->sessionId;
    hSession->sessionId = handshake.sessionId;
    hSession->connectKey = handshake.connectKey;
    hSession->handshakeOK = false;
    AdminSession(OP_UPDATE, unOld, hSession);
#ifdef HDC_SUPPORT_UART
    if (hSession->connType == CONN_SERIAL and clsUARTServ!= nullptr) {
        WRITE_LOG(LOG_DEBUG, " HdcDaemon::DaemonSessionHandshake %s",
                    handshake.ToDebugString().c_str());
        if (clsUARTServ != nullptr) {
            (static_cast<HdcDaemonUART *>(clsUARTServ))->OnNewHandshakeOK(hSession->sessionId);
        }
    } else
#endif // HDC_SUPPORT_UART
    if (clsUSBServ != nullptr) {
        (reinterpret_cast<HdcDaemonUSB *>(clsUSBServ))->OnNewHandshakeOK(hSession->sessionId);
    }

    handshake.sessionId = 0;
    handshake.connectKey = "";
}

bool HdcDaemon::DaemonSessionHandshake(HSession hSession, const uint32_t channelId, uint8_t *payload, int payloadSize)
{
    StartTraceScope("HdcDaemon::DaemonSessionHandshake");
    // session handshake step2
    string s = string(reinterpret_cast<char *>(payload), payloadSize);
    SessionHandShake handshake;
    string err;
    SerialStruct::ParseFromString(handshake, s);
#ifdef HDC_DEBUG
    WRITE_LOG(LOG_DEBUG, "session %s try to handshake", hSession->ToDebugString().c_str());
#endif
    // banner to check is parse ok...
    if (handshake.banner != HANDSHAKE_MESSAGE) {
        hSession->availTailIndex = 0;
        WRITE_LOG(LOG_FATAL, "Recv server-hello failed");
        return false;
    }
    if (handshake.authType == AUTH_NONE) {
        DaemonSessionHandshakeInit(hSession, handshake);
    }
    if (!HandDaemonAuth(hSession, channelId, handshake)) {
        WRITE_LOG(LOG_FATAL, "auth failed");
        return false;
    }
    string version = Base::GetVersion() + HDC_MSG_HASH;

    WRITE_LOG(LOG_DEBUG, "receive hs version = %s", handshake.version.c_str());

    if (!handshake.version.empty() && handshake.version != version) {
        WRITE_LOG(LOG_FATAL, "DaemonSessionHandshake failed! version not match [%s] vs [%s]",
            handshake.version.c_str(), version.c_str());
#ifdef HDC_CHECK_CHECK
        hSession->availTailIndex = 0;
        handshake.banner = HANDSHAKE_FAILED;
        string failedString = SerialStruct::SerializeToString(handshake);
        Send(hSession->sessionId, channelId, CMD_KERNEL_HANDSHAKE, (uint8_t *)failedString.c_str(),
             failedString.size());
        return false;
#endif
    }
    if (handshake.version.empty()) {
        handshake.version = Base::GetVersion();
        WRITE_LOG(LOG_FATAL, "set version if check mode = %s", handshake.version.c_str());
    }
    // handshake auth OK.Can append the sending device information to HOST
#ifdef HDC_DEBUG
    WRITE_LOG(LOG_INFO, "session %u handshakeOK send back CMD_KERNEL_HANDSHAKE", hSession->sessionId);
#endif
    hSession->handshakeOK = true;
    return true;
}

bool HdcDaemon::IsExpectedParam(const string& param, const string& expect)
{
    string out;
    SystemDepend::GetDevItem(param.c_str(), out);
    return (out.empty() || out == expect); // default empty
}

bool HdcDaemon::CheckControl(const uint16_t command)
{
    bool ret = false; // default no debug
    switch (command) { // this switch is match RedirectToTask function
        case CMD_UNITY_EXECUTE:
        case CMD_UNITY_REMOUNT:
        case CMD_UNITY_REBOOT:
        case CMD_UNITY_RUNMODE:
        case CMD_UNITY_HILOG:
        case CMD_UNITY_ROOTRUN:
        case CMD_UNITY_TERMINATE:
        case CMD_UNITY_BUGREPORT_INIT:
        case CMD_JDWP_LIST:
        case CMD_JDWP_TRACK:
        case CMD_SHELL_INIT:
        case CMD_SHELL_DATA: {
            ret = IsExpectedParam("persist.hdc.control.shell", "true");
            break;
        }
        case CMD_FILE_CHECK:
        case CMD_FILE_DATA:
        case CMD_FILE_FINISH:
        case CMD_FILE_INIT:
        case CMD_FILE_BEGIN:
        case CMD_FILE_MODE:
        case CMD_DIR_MODE:
        case CMD_APP_CHECK:
        case CMD_APP_DATA:
        case CMD_APP_UNINSTALL: {
            ret = IsExpectedParam("persist.hdc.control.file", "true");
            break;
        }
        case CMD_FORWARD_INIT:
        case CMD_FORWARD_CHECK:
        case CMD_FORWARD_ACTIVE_MASTER:
        case CMD_FORWARD_ACTIVE_SLAVE:
        case CMD_FORWARD_DATA:
        case CMD_FORWARD_FREE_CONTEXT:
        case CMD_FORWARD_CHECK_RESULT: {
            ret = IsExpectedParam("persist.hdc.control.fport", "true");
            break;
        }
        default:
            ret = true; // other ECHO_RAW and so on
    }
    return ret;
}

bool HdcDaemon::FetchCommand(HSession hSession, const uint32_t channelId, const uint16_t command, uint8_t *payload,
                             const int payloadSize)
{
    StartTraceScope("HdcDaemon::FetchCommand");
    bool ret = true;
    if (enableSecure && (GetSessionAuthStatus(hSession->sessionId) != AUTH_OK) &&
        command != CMD_KERNEL_HANDSHAKE && command != CMD_KERNEL_CHANNEL_CLOSE) {
        string authmsg = GetSessionAuthmsg(hSession->sessionId);
        WRITE_LOG(LOG_WARN, "session %u auth failed: %s for command %u",
                  hSession->sessionId, authmsg.c_str(), command);
        if (!authmsg.empty()) {
            LogMsg(hSession->sessionId, channelId, MSG_FAIL, authmsg.c_str());
        }
        uint8_t count = 1;
        Send(hSession->sessionId, channelId, CMD_KERNEL_CHANNEL_CLOSE, &count, 1);
        return true;
    }
    if (command != CMD_UNITY_BUGREPORT_DATA &&
        command != CMD_SHELL_DATA &&
        command != CMD_FORWARD_DATA &&
        command != CMD_FILE_DATA &&
        command != CMD_APP_DATA) {
        WRITE_LOG(LOG_DEBUG, "FetchCommand channelId:%u command:%u", channelId, command);
    }
    switch (command) {
        case CMD_KERNEL_HANDSHAKE: {
            // session handshake step2
            ret = DaemonSessionHandshake(hSession, channelId, payload, payloadSize);
            break;
        }
        case CMD_KERNEL_CHANNEL_CLOSE: {  // Daemon is only cleaning up the Channel task
            ClearOwnTasks(hSession, channelId);
            if (*payload != 0) {
                --(*payload);
                Send(hSession->sessionId, channelId, CMD_KERNEL_CHANNEL_CLOSE, payload, 1);
            }
            ret = true;
            break;
        }
        default:
            ret = true;
            if (CheckControl(command)) {
                ret = DispatchTaskData(hSession, channelId, command, payload, payloadSize);
            } else {
                LogMsg(hSession->sessionId, channelId, MSG_FAIL, "debugging is not allowed");
                uint8_t count = 1;
                Send(hSession->sessionId, channelId, CMD_KERNEL_CHANNEL_CLOSE, &count, 1);
            }
            break;
    }
    return ret;
}

bool HdcDaemon::RemoveInstanceTask(const uint8_t op, HTaskInfo hTask)
{
    bool ret = true;

    if (!hTask->taskClass) {
        return ret;
    }

    switch (hTask->taskType) {
        case TYPE_UNITY:
            ret = DoTaskRemove<HdcDaemonUnity>(hTask, op);
            break;
        case TYPE_SHELL:
            ret = DoTaskRemove<HdcShell>(hTask, op);
            break;
        case TASK_FILE:
            ret = DoTaskRemove<HdcTransferBase>(hTask, op);
            break;
        case TASK_FORWARD:
            ret = DoTaskRemove<HdcDaemonForward>(hTask, op);
            break;
        case TASK_APP:
            ret = DoTaskRemove<HdcDaemonApp>(hTask, op);
            break;
        default:
            ret = false;
            break;
    }
    return ret;
}

bool HdcDaemon::ServerCommand(const uint32_t sessionId, const uint32_t channelId, const uint16_t command,
                              uint8_t *bufPtr, const int size)
{
    return Send(sessionId, channelId, command, reinterpret_cast<uint8_t *>(bufPtr), size) > 0;
}

void HdcDaemon::JdwpNewFileDescriptor(const uint8_t *buf, const int bytesIO)
{
    uint8_t spcmd = *const_cast<uint8_t *>(buf);
    if (spcmd == SP_JDWP_NEWFD) {
        int cnt = sizeof(uint8_t) + sizeof(uint32_t) * 2;
        if (bytesIO < cnt) {
            WRITE_LOG(LOG_FATAL, "jdwp newfd data insufficient bytesIO:%d", bytesIO);
            return;
        }
        uint32_t pid = *reinterpret_cast<uint32_t *>(const_cast<uint8_t *>(buf + 1));
        uint32_t fd = *reinterpret_cast<uint32_t *>(const_cast<uint8_t *>(buf + 5));  // 5 : fd offset
        ((HdcJdwp *)clsJdwp)->SendJdwpNewFD(pid, fd);
    } else if (spcmd == SP_ARK_NEWFD) {
        // SP_ARK_NEWFD | fd[1] | ark:pid@tid@Debugger
        int cnt = sizeof(uint8_t) + sizeof(uint32_t);
        if (bytesIO < cnt) {
            WRITE_LOG(LOG_FATAL, "ark newfd data insufficient bytesIO:%d", bytesIO);
            return;
        }
        int32_t fd = *reinterpret_cast<int32_t *>(const_cast<uint8_t *>(buf + 1));
        std::string arkstr = std::string(
            reinterpret_cast<char *>(const_cast<uint8_t *>(buf + 5)), bytesIO - 5);  // 5 : fd offset
        WRITE_LOG(LOG_DEBUG, "JdwpNewFileDescriptor arkstr:%s fd:%d", arkstr.c_str(), fd);
        ((HdcJdwp *)clsJdwp)->SendArkNewFD(arkstr, fd);
    }
}

void HdcDaemon::NotifyInstanceSessionFree(HSession hSession, bool freeOrClear)
{
    if (!freeOrClear) {
        WRITE_LOG(LOG_WARN, "NotifyInstanceSessionFree freeOrClear false");
        return;  // ignore step 1
    }
    if (clsUSBServ != nullptr) {
        auto clsUsbModule = reinterpret_cast<HdcDaemonUSB *>(clsUSBServ);
        clsUsbModule->OnSessionFreeFinally(hSession);
    }
}

void HdcDaemon::InitSessionAuthInfo(uint32_t sessionid, string token)
{
    HdcDaemonAuthInfo info = {
        AUTH_NONE,
        token
    };
    mapAuthStatusMutex.lock();
    mapAuthStatus[sessionid] = info;
    mapAuthStatusMutex.unlock();
}
void HdcDaemon::UpdateSessionAuthOk(uint32_t sessionid)
{
    HdcDaemonAuthInfo info;
    mapAuthStatusMutex.lock();
    info = mapAuthStatus[sessionid];
    info.authtype = AUTH_OK;
    info.token = "";
    info.pubkey = "";
    mapAuthStatus[sessionid] = info;
    mapAuthStatusMutex.unlock();
}
void HdcDaemon::UpdateSessionAuthPubkey(uint32_t sessionid, string pubkey)
{
    HdcDaemonAuthInfo info;
    mapAuthStatusMutex.lock();
    info = mapAuthStatus[sessionid];
    info.authtype = AUTH_PUBLICKEY;
    info.pubkey = pubkey;
    mapAuthStatus[sessionid] = info;
    mapAuthStatusMutex.unlock();
}
void HdcDaemon::UpdateSessionAuthmsg(uint32_t sessionid, string authmsg)
{
    HdcDaemonAuthInfo info;
    mapAuthStatusMutex.lock();
    info = mapAuthStatus[sessionid];
    info.authtype = AUTH_FAIL;
    info.authmsg = authmsg;
    mapAuthStatus[sessionid] = info;
    mapAuthStatusMutex.unlock();
}
void HdcDaemon::DeleteSessionAuthStatus(uint32_t sessionid)
{
    mapAuthStatusMutex.lock();
    mapAuthStatus.erase(sessionid);
    mapAuthStatusMutex.unlock();
}
HdcSessionBase::AuthType HdcDaemon::GetSessionAuthStatus(uint32_t sessionid)
{
    HdcDaemonAuthInfo info;

    mapAuthStatusMutex.lock();
    info = mapAuthStatus[sessionid];
    mapAuthStatusMutex.unlock();

    return info.authtype;
}
string HdcDaemon::GetSessionAuthToken(uint32_t sessionid)
{
    HdcDaemonAuthInfo info;

    mapAuthStatusMutex.lock();
    info = mapAuthStatus[sessionid];
    mapAuthStatusMutex.unlock();

    return info.token;
}
string HdcDaemon::GetSessionAuthPubkey(uint32_t sessionid)
{
    HdcDaemonAuthInfo info;

    mapAuthStatusMutex.lock();
    info = mapAuthStatus[sessionid];
    mapAuthStatusMutex.unlock();

    return info.pubkey;
}
string HdcDaemon::GetSessionAuthmsg(uint32_t sessionid)
{
    HdcDaemonAuthInfo info;

    mapAuthStatusMutex.lock();
    info = mapAuthStatus[sessionid];
    mapAuthStatusMutex.unlock();

    return info.authmsg;
}
void HdcDaemon::SendAuthOkMsg(SessionHandShake &handshake, uint32_t channelid,
                              uint32_t sessionid, string msg, string daemonAuthResult)
{
    char hostname[BUF_SIZE_MEDIUM] = { 0 };
    if (gethostname(hostname, BUF_SIZE_MEDIUM) != 0) {
        WRITE_LOG(LOG_FATAL, "get hostname failed %s", strerror(errno));
    }
    if (handshake.version < "Ver: 3.0.0b") {
        if (msg.empty()) {
            msg = hostname;
        }
        handshake.buf = msg;
    } else {
        string emgmsg;
        Base::TlvAppend(emgmsg, TAG_EMGMSG, msg);
        Base::TlvAppend(emgmsg, TAG_DEVNAME, hostname);
        Base::TlvAppend(emgmsg, TAG_DAEOMN_AUTHSTATUS, daemonAuthResult);
        handshake.buf = emgmsg;
    }

    handshake.authType = AUTH_OK;
    string bufString = SerialStruct::SerializeToString(handshake);
    Send(sessionid, channelid, CMD_KERNEL_HANDSHAKE,
            reinterpret_cast<uint8_t *>(const_cast<char *>(bufString.c_str())), bufString.size());
    uint8_t count = 1;
    Send(sessionid, channelid, CMD_KERNEL_CHANNEL_CLOSE, &count, 1);
}
void HdcDaemon::SendAuthSignMsg(SessionHandShake &handshake,
        uint32_t channelId, uint32_t sessionid, string pubkey, string token)
{
    UpdateSessionAuthPubkey(sessionid, pubkey);
    handshake.authType = AUTH_SIGNATURE;
    handshake.buf = token;
    string bufString = SerialStruct::SerializeToString(handshake);
    Send(sessionid, channelId, CMD_KERNEL_HANDSHAKE,
            reinterpret_cast<uint8_t *>(const_cast<char *>(bufString.c_str())), bufString.size());
}
void HdcDaemon::EchoHandshakeMsg(SessionHandShake &handshake, uint32_t channelid, uint32_t sessionid, string msg)
{
    SendAuthOkMsg(handshake, channelid, sessionid, msg, DAEOMN_UNAUTHORIZED);
    LogMsg(sessionid, channelid, MSG_FAIL, msg.c_str());
    UpdateSessionAuthmsg(sessionid, msg);
}
void HdcDaemon::AuthRejectLowClient(SessionHandShake &handshake, uint32_t channelid, uint32_t sessionid)
{
    string msg = "[E000001]:The sdk hdc.exe version is too low, please upgrade to the latest version.";
    EchoHandshakeMsg(handshake, channelid, sessionid, msg);
}
}  // namespace Hdc
