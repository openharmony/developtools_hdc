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
#include "connect_validation.h"
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
#ifdef HDC_SUPPORT_REPORT_COMMAND_EVENT
#include "command_event_report.h"
#endif
#include "hdc_statistic_reporter.h"

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
    authEnable = false;
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
        clsBridgeServ = nullptr;
    }
#endif
#ifdef HDC_SUPPORT_UART
    if (clsUARTServ) {
        delete (HdcDaemonUART *)clsUARTServ;
        clsUARTServ = nullptr;
    }
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

bool HdcDaemon::GetAuthByPassValue()
{
    int connectValidationStatus = HdcValidation::GetConnectValidationParam();
    if ((connectValidationStatus == VALIDATION_HDC_DAEMON
        || connectValidationStatus == VALIDATION_HDC_HOST_AND_DAEMON)) {
        WRITE_LOG(LOG_INFO, "If the connection verification function is enabled on the device side, "
            "authentication is required.");
        return true;
    }

    // enable security
    string secure;
#ifdef CONFIG_HDC_OSPM_AUTH_DISABLE
    WRITE_LOG(LOG_INFO, "Get param secure failed caused by hdc ospm auth disable");
    return false;
#endif
    if (!SystemDepend::GetDevItem("const.hdc.secure", secure)) {
        WRITE_LOG(LOG_INFO, "Get param secure failed");
        return true;
    }
    if (Base::Trim(secure) != "1") {
        WRITE_LOG(LOG_INFO, "Authentication is not required in root mode");
        return false;
    }

    string oemmode;
    if (!SystemDepend::GetDevItem("const.boot.oemmode", oemmode)) {
        WRITE_LOG(LOG_INFO, "Get param oemmode failed");
        return true;
    }
    if (Base::Trim(oemmode) != "rd") {
        WRITE_LOG(LOG_INFO, "The device is locked, Authentication is required");
        return true;
    }

    string authbypass;
    if (!SystemDepend::GetDevItem("persist.hdc.auth_bypass", authbypass)) {
        WRITE_LOG(LOG_INFO, "Get param auth_bypass failed");
        return true;
    }
    if (Base::Trim(authbypass) == "1") {
        WRITE_LOG(LOG_INFO, "Auth bypass, allow access");
        return false;
    }

    return true;
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
    clsJdwp = new(std::nothrow) HdcJdwp(&loopMain, &loopMainStatus);
    if (clsJdwp == nullptr) {
        WRITE_LOG(LOG_FATAL, "InitMod new clsJdwp failed");
        return;
    }
    ((HdcJdwp *)clsJdwp)->Initial();
#ifndef HDC_EMULATOR
    authEnable = GetAuthByPassValue();
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
bool HdcDaemon::RedirectToTask(HTaskInfo hTaskInfo, HSession /* hSession */, const uint32_t /* channelId */,
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
        case CMD_UNITY_BUGREPORT_INIT:
        case CMD_JDWP_LIST:
        case CMD_JDWP_TRACK:
        case CMD_UNITY_EXECUTE_EX:
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
    if (pipe(fds) != 0) {
        WRITE_LOG(LOG_FATAL, "pipe failed %s", strerror(errno));
        return false;
    }

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
        if (ret < 0) {
            // if execl failed need return false
            WRITE_LOG(LOG_FATAL, "start user_permit failed %d: %s", ret, strerror(errno));
            _exit(0);
        }
    } else {
            Base::CloseFd(fds[1]);
            waitpid(pid, nullptr, 0);
            char buf[1024] = { 0 };
            int nbytes = read(fds[0], buf, sizeof(buf) - 1);
            WRITE_LOG(LOG_FATAL, "user_permit put %d bytes: %s", nbytes, buf);
            close(fds[0]);
    }

    return true;
}

UserPermit HdcDaemon::PostUIConfirm(string hostname, string pubkey)
{
    // clear result first
    if (!SystemDepend::SetDevItem("persist.hdc.daemon.auth_result", "auth_result_none")) {
        WRITE_LOG(LOG_FATAL, "debug auth result failed, so refuse this connect");
        return REFUSE;
    }

    // then write para for setting
    if (!SystemDepend::SetDevItem("persist.hdc.client.hostname", hostname.c_str())) {
        WRITE_LOG(LOG_FATAL, "set param(%s) failed", Hdc::MaskString(hostname).c_str());
        return REFUSE;
    }

    uint8_t sha256Result[SHA256_DIGEST_LENGTH] = { 0 };
    if (SHA256(reinterpret_cast<const uint8_t *>(pubkey.c_str()), pubkey.length(), sha256Result) == nullptr) {
        WRITE_LOG(LOG_FATAL, "sha256 pubkey failed");
        return REFUSE;
    }

    string hex = Base::Convert2HexStr(sha256Result, SHA256_DIGEST_LENGTH);
    if (!SystemDepend::SetDevItem("persist.hdc.client.pubkey_sha256", hex.c_str())) {
        WRITE_LOG(LOG_DEBUG, "Failed to set pubkey prop.");
        return REFUSE;
    }

    if (!ShowPermitDialog()) {
        WRITE_LOG(LOG_FATAL, "show dialog failed, so refuse this connect.");
        return REFUSE;
    }

    string authResult;
    if (!SystemDepend::GetDevItem("persist.hdc.daemon.auth_result", authResult)) {
        WRITE_LOG(LOG_FATAL, "user refuse [%s] this developer [%s]",
            authResult.c_str(), Hdc::MaskString(hostname).c_str());
        return REFUSE;
    }
    WRITE_LOG(LOG_FATAL, "user permit_result [%s] for this developer [%s]",
            authResult.c_str(), Hdc::MaskString(hostname).c_str());
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

    size_t tmp = buf.find(separator);
    if (tmp == string::npos) {
        WRITE_LOG(LOG_FATAL, "get hostname or pubkey failed");
        return false;
    }
    hostname = buf.substr(0, tmp);
    pubkey = buf.substr(tmp + 1);
    WRITE_LOG(LOG_INFO, "hostname is [%s], pubkey is [%s]", Hdc::MaskString(hostname).c_str(),
        Hdc::MaskString(pubkey.substr(0, pubkey.size() / 2)).c_str());

    return (!hostname.empty() && !pubkey.empty());
}

void HdcDaemon::ClearKnownHosts()
{
    char const *keyfile = "/data/service/el1/public/hdc/hdc_keys";

    if (!authEnable || HandDaemonAuthBypass()) {
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
    char const *keyfile = "/data/service/el1/public/hdc/hdc_keys";

    std::ofstream keyofs(keyfile, std::ios::app);
    if (!keyofs.is_open()) {
        WRITE_LOG(LOG_FATAL, "open keyfile %s error", keyfile);
        return;
    }

    string keytmp = key + "\n";
    keyofs.write(keytmp.c_str(), keytmp.length());
    keyofs.flush();
    keyofs.close();

    WRITE_LOG(LOG_FATAL, "save new key [%s] into keyfile %s over",
              Hdc::MaskString(key.substr(0, key.size() / 2)).c_str(), keyfile);

    return;
}

bool HdcDaemon::AlreadyInKnownHosts(const string& key)
{
    char const *keyfile = "/data/service/el1/public/hdc/hdc_keys";

    std::ifstream keyifs(keyfile);
    if (!keyifs.is_open()) {
        WRITE_LOG(LOG_FATAL, "open keyfile %s error", keyfile);
        return false;
    }

    std::string keys((std::istreambuf_iterator<char>(keyifs)), std::istreambuf_iterator<char>());
    // the length of pubkey is 625
    const int keyLength = 625;
    if (key.size() == keyLength && keys.find(key) != string::npos) {
        keyifs.close();
        return true;
    }

    WRITE_LOG(LOG_FATAL, "key [%s] not in keyfile %s", Hdc::MaskString(key.substr(0, key.size() / 2)).c_str(), keyfile);

    keyifs.close();
    return false;
}

bool HdcDaemon::HandDaemonAuthInit(HSession hSession, const uint32_t channelId, SessionHandShake &handshake)
{
    hSession->tokenRSA = Base::GetSecureRandomString(SHA_DIGEST_LENGTH);
    handshake.authType = AUTH_PUBLICKEY;
    /*
     * If we know client support RSA_3072_SHA512 in AUTH_NONE phase
     * Then told client that the server also support RSA_3072_SHA512 auth
     * Notice, before here is "handshake.buf = hSession->tokenRSA", but the server not use it
    */
    if (hSession->verifyType == AuthVerifyType::RSA_3072_SHA512) {
        handshake.buf.clear();
        Base::TlvAppend(handshake.buf, TAG_AUTH_TYPE, std::to_string(AuthVerifyType::RSA_3072_SHA512));
        WRITE_LOG(LOG_INFO, "client support RSA_3072_SHA512 auth for %s session",
            Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
    }
    string bufString = SerialStruct::SerializeToString(handshake);
    Send(hSession->sessionId, channelId, CMD_KERNEL_HANDSHAKE,
            reinterpret_cast<uint8_t *>(const_cast<char *>(bufString.c_str())),
            bufString.size());

    InitSessionAuthInfo(hSession->sessionId, hSession->tokenRSA);
    return true;
}

bool HdcDaemon::HandConnectValidationPubkey(HSession hSession, const uint32_t channelId, SessionHandShake &handshake)
{
    string hostname, pubkey;
    // parse recv pubkey
    if (!GetHostPubkeyInfo(handshake.buf, hostname, pubkey)) {
        WRITE_LOG(LOG_FATAL, "get pubkey failed for %u", hSession->sessionId);
        return false;
    }
 
    if (HdcValidation::CheckAuthPubKeyIsValid(pubkey)) {
        SendAuthMsg(handshake, channelId, hSession, pubkey);
    } else {
        string notifymsg = "[E000010]:Auth failed, cannt login the device.";
        WRITE_LOG(LOG_FATAL, "%s", notifymsg.c_str());
        HandleAuthFailed(handshake, channelId, hSession, notifymsg);
    }
    return true;
}

bool HdcDaemon::HandDaemonAuthPubkey(HSession hSession, const uint32_t channelId, SessionHandShake &handshake)
{
    int connectValidationStatus = HdcValidation::GetConnectValidationParam();
    if (connectValidationStatus == VALIDATION_HDC_DAEMON
        || connectValidationStatus == VALIDATION_HDC_HOST_AND_DAEMON) {
        return HandConnectValidationPubkey(hSession, channelId, handshake);
    }
    bool ret = false;
    string hostname, pubkey;
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hSession->sessionId);

    do {
        if (!GetHostPubkeyInfo(handshake.buf, hostname, pubkey)) {
            WRITE_LOG(LOG_FATAL, "get pubkey failed for %s", sessionIdMaskStr.c_str());
            break;
        }
        if (AlreadyInKnownHosts(pubkey)) {
            ret = true;
            break;
        }

        std::thread notifymsg([this, &handshake, channelId, &hSession]() {
            std::string confirmmsg = "[E000002]:The device unauthorized.\r\n"\
                                     "This server's public key is not set.\r\n"\
                                     "Please check for a confirmation dialog on your device.\r\n"\
                                     "Otherwise try 'hdc kill' if that seems wrong.";
            this->HandleAuthFailed(handshake, channelId, hSession, confirmmsg);
        });
        notifymsg.detach();

        UserPermit permit = PostUIConfirm(hostname, pubkey);
        if (permit == ALLOWONCE) {
            WRITE_LOG(LOG_FATAL, "user allow onece for %s", sessionIdMaskStr.c_str());
            ret = true;
        } else if (permit == ALLOWFORVER) {
            WRITE_LOG(LOG_FATAL, "user allow forever for %s", sessionIdMaskStr.c_str());
            UpdateKnownHosts(pubkey);
            ret = true;
        } else {
            WRITE_LOG(LOG_FATAL, "user refuse for %s", sessionIdMaskStr.c_str());
            ret = false;
        }
    } while (0);

    if (ret) {
        SendAuthMsg(handshake, channelId, hSession, pubkey);
    } else {
        string notifymsg = "[E000003]:The device unauthorized.\r\n"\
                            "The user denied the access for the device.\r\n"\
                             "Please execute 'hdc kill' and redo your command,\r\n"\
                             "then check for a confirmation dialog on your device.";
        HandleAuthFailed(handshake, channelId, hSession, notifymsg);
    }
    return true;
}

/*
 * tokenSignBase64 is Base64 encode of the signing from server
 * token is the source data same of server for signing
 */
bool HdcDaemon::RsaSignVerify(HSession hSession, EVP_PKEY_CTX *ctx, const string &tokenSignBase64, const string &token)
{
    unsigned char tokenSha512[SHA512_DIGEST_LENGTH];
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hSession->sessionId);
    try {
        // Base64 divides every 3 bytes(8 bits) of the original data into 4 segments(6 bits),
        const int scaleOfEncrypt = 4;
        const int scaleOfDecrypt = 3;
        // Invalid base64 format
        const int base64BlockSize = 4; // base64 string must be a multiple of 4
        if (tokenSignBase64.empty() || tokenSignBase64.length() % base64BlockSize != 0) {
            WRITE_LOG(LOG_FATAL, "Invalid base64 format for session %s", sessionIdMaskStr.c_str());
            return false;
        }
        size_t maxDecodedLen = (tokenSignBase64.length() / scaleOfEncrypt * scaleOfDecrypt);
        std::unique_ptr<unsigned char[]> tokenRsaSign = std::make_unique<unsigned char[]>(maxDecodedLen);
        // Get the real token sign
        int tokenRsaSignLen = EVP_DecodeBlock(tokenRsaSign.get(),
            reinterpret_cast<const unsigned char *>(tokenSignBase64.c_str()), tokenSignBase64.length());
        if (tokenRsaSignLen <= 0) {
            WRITE_LOG(LOG_FATAL, "base64 decode token sign failed for session %s", sessionIdMaskStr.c_str());
            return false;
        }
        SHA512(reinterpret_cast<const unsigned char *>(token.c_str()), token.size(), tokenSha512);
        if (EVP_PKEY_verify(ctx, tokenRsaSign.get(), tokenRsaSignLen, tokenSha512, sizeof(tokenSha512)) <= 0) {
            WRITE_LOG(LOG_FATAL, "verify failed for session %s", sessionIdMaskStr.c_str());
            return false;
        }
    } catch (std::exception &e) {
        WRITE_LOG(LOG_FATAL, "sign verify failed for session %s with exception %s",
            sessionIdMaskStr.c_str(), e.what());
        return false;
    }

    WRITE_LOG(LOG_FATAL, "sign verify success for session %s", sessionIdMaskStr.c_str());
    return true;
}

bool HdcDaemon::AuthVerifyRsaSign(HSession hSession, const string &tokenSign, const string &token, RSA *rsa)
{
    EVP_PKEY *signKey = nullptr;
    EVP_PKEY_CTX *ctx = nullptr;
    bool signRet = false;

    signKey = EVP_PKEY_new();
    if (signKey == nullptr) {
        WRITE_LOG(LOG_FATAL, "EVP_PKEY_new failed");
        return false;
    }
    do {
        if (EVP_PKEY_set1_RSA(signKey, rsa) <= 0) {
            WRITE_LOG(LOG_FATAL, "EVP_PKEY_new failed");
            break;
        }
        // the length of vaild sign result for BASE64 can't bigger than  EVP_PKEY_size(signKey) * 2
        if (tokenSign.size() > ((size_t)EVP_PKEY_size(signKey) * (size_t)2)) {
            WRITE_LOG(LOG_FATAL, "invalid base64 sign size %zd for session %s", tokenSign.size(),
            Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
            break;
        }
        ctx = EVP_PKEY_CTX_new(signKey, nullptr);
        if (ctx == nullptr) {
            WRITE_LOG(LOG_FATAL, "EVP_PKEY_CTX_new failed");
            break;
        }
        if (EVP_PKEY_verify_init(ctx) <= 0) {
            WRITE_LOG(LOG_FATAL, "EVP_PKEY_CTX_new failed");
            break;
        }
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PSS_PADDING) <= 0 ||
            EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, RSA_PSS_SALTLEN_AUTO) <= 0) {
            WRITE_LOG(LOG_FATAL, "set saltlen or padding failed");
            break;
        }
        if (EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha512()) <= 0) {
            WRITE_LOG(LOG_FATAL, "EVP_PKEY_CTX_set_signature_md failed");
            break;
        }
        signRet = RsaSignVerify(hSession, ctx, tokenSign, token);
    } while (0);

    if (ctx != nullptr) {
        EVP_PKEY_CTX_free(ctx);
    }
    if (signKey != nullptr) {
        EVP_PKEY_free(signKey);
    }
    return signRet;
}

bool HdcDaemon::AuthVerify(HSession hSession, const string &encryptToken, const string &token, const string &pubkey)
{
    BIO *bio = nullptr;
    RSA *rsa = nullptr;
    const unsigned char *pubkeyp = reinterpret_cast<const unsigned char *>(pubkey.c_str());
    bool verifyResult = false;
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hSession->sessionId);

    do {
        bio = BIO_new(BIO_s_mem());
        if (bio == nullptr) {
            WRITE_LOG(LOG_FATAL, "bio failed for session %s", sessionIdMaskStr.c_str());
            break;
        }
        int wbytes = BIO_write(bio, pubkeyp, pubkey.length());
        if (wbytes != static_cast<int>(pubkey.length())) {
            WRITE_LOG(LOG_FATAL, "bio write failed %d for session %s", wbytes, sessionIdMaskStr.c_str());
            break;
        }
        rsa = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
        if (rsa == nullptr) {
            WRITE_LOG(LOG_FATAL, "rsa failed for session %s", sessionIdMaskStr.c_str());
            break;
        }
        if (hSession->verifyType == AuthVerifyType::RSA_3072_SHA512) {
            verifyResult = AuthVerifyRsaSign(hSession, encryptToken, token, rsa);
        } else {
            verifyResult = AuthVerifyRsa(hSession, encryptToken, token, rsa);
        }
    } while (0);

    if (bio) {
        BIO_free(bio);
    }
    if (rsa) {
        RSA_free(rsa);
    }

    return verifyResult;
}

bool HdcDaemon::AuthVerifyRsa(HSession hSession, const string &encryptToken, const string &token, RSA *rsa)
{
    const unsigned char *tokenp = reinterpret_cast<const unsigned char *>(encryptToken.c_str());
    unsigned char tokenDecode[BUF_SIZE_DEFAULT] = { 0 };
    unsigned char decryptToken[BUF_SIZE_DEFAULT] = { 0 };

    // for rsa encrypt, the length of encryptToken can't bigger than BUF_SIZE_DEFAULT
    // Base64 divides every 3 bytes(8 bits) of the original data into 4 segments(6 bits),
    // each of which is then mapped to a new character.
    const int scaleOfEncrypt = 4;
    const int scaleOfDecode = 3;
    if (encryptToken.length() > BUF_SIZE_DEFAULT / scaleOfDecode * scaleOfEncrypt) {
        WRITE_LOG(LOG_FATAL, "invalid encryptToken, length is %zd", encryptToken.length());
        return false;
    }
    int tbytes = EVP_DecodeBlock(tokenDecode, tokenp, encryptToken.length());
    if (tbytes <= 0) {
        WRITE_LOG(LOG_FATAL, "base64 decode pubkey failed");
        return false;
    }
    int bytes = RSA_public_decrypt(tbytes, tokenDecode, decryptToken, rsa, RSA_PKCS1_PADDING);
    if (bytes < 0) {
        WRITE_LOG(LOG_FATAL, "decrypt failed(%lu) for session %s", ERR_get_error(),
        Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        return false;
    }
    string sdecryptToken(reinterpret_cast<const char *>(decryptToken), bytes);
    if (sdecryptToken != token) {
        WRITE_LOG(LOG_FATAL, "auth failed for session %s)",
            Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        return false;
    }
    return true;
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
    string token = GetSessionAuthToken(hSession->sessionId);
    string pubkey = GetSessionAuthPubkey(hSession->sessionId);
    if (!AuthVerify(hSession, handshake.buf, token, pubkey)) {
        WRITE_LOG(LOG_FATAL, "auth failed for session %s",
            Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        // Next auth
        HandleAuthFailed(handshake, channelId, hSession, "[E000010]:Auth failed, cannt login the device.");
        return true;
    }

    WRITE_LOG(LOG_FATAL, "auth success for session %s",
        Hdc::MaskSessionIdToString(hSession->sessionId).c_str());

    UpdateSessionAuthOk(hSession->sessionId);
    SendAuthOkMsg(handshake, channelId, hSession);
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
    int connectValidationStatus = HdcValidation::GetConnectValidationParam();
    bool connectstatus = (connectValidationStatus == VALIDATION_HDC_DAEMON
        || connectValidationStatus == VALIDATION_HDC_HOST_AND_DAEMON);
 
    if (!connectstatus && !authEnable) {
        WRITE_LOG(LOG_INFO, "not enable secure, allow access for %s",
            Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        UpdateSessionAuthOk(hSession->sessionId);
        SendAuthOkMsg(handshake, channelId, hSession);
        return true;
    } else if (handshake.version < "Ver: 3.0.0b") {
        WRITE_LOG(LOG_INFO, "session %s client version %s is too low,authType %d",
                    Hdc::MaskSessionIdToString(hSession->sessionId).c_str(),
                    handshake.version.c_str(), handshake.authType);
        AuthRejectLowClient(handshake, channelId, hSession);
        return true;
    } else if (connectstatus && !hSession->supportConnValidation) {
        WRITE_LOG(LOG_INFO, "session %s client is not support connect validation",
                  Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        AuthRejectNotSupportConnValidation(handshake, channelId, hSession);
        return true;
    } else if (GetSessionAuthStatus(hSession->sessionId) == AUTH_OK) {
        WRITE_LOG(LOG_INFO, "session %u already auth ok", hSession->sessionId);
        return true;
    }

    if (handshake.authType == AUTH_NONE) {
        return HandDaemonAuthInit(hSession, channelId, handshake);
    } else if (GetSessionAuthStatus(hSession->sessionId) == AUTH_NONE
        && GetSessionAuthToken(hSession->sessionId).size() > 0
        && handshake.authType == AUTH_PUBLICKEY) {
        return HandDaemonAuthPubkey(hSession, channelId, handshake);
    } else if (GetSessionAuthStatus(hSession->sessionId) == AUTH_PUBLICKEY && handshake.authType == AUTH_SIGNATURE) {
        return HandDaemonAuthSignature(hSession, channelId, handshake);
#ifdef HDC_SUPPORT_ENCRYPT_TCP
    } else if (GetSessionAuthStatus(hSession->sessionId) == AUTH_PUBLICKEY && handshake.authType == AUTH_SSL_TLS_PSK) {
        return DaemonSSLHandshake(hSession, channelId, handshake);
#endif
    } else {
        WRITE_LOG(LOG_FATAL, "invalid auth state %d for session %s", handshake.authType,
            Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        return false;
    }
}

/*
 * For daemon, if server add new capability, we can parse it here
 */
void HdcDaemon::GetServerCapability(HSession &hSession, SessionHandShake &handshake)
{
    /*
     * Check if server support RSA_3072_SHA512 for auth
     * if the value not contain RSA_3072_SHA512, We treat it not support
    */
    std::map<string, string> tlvMap;
    hSession->verifyType = AuthVerifyType::RSA_ENCRYPT;
    if (!Base::TlvToStringMap(handshake.buf, tlvMap)) {
        WRITE_LOG(LOG_INFO, "maybe old version client for %s session",
            Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
        return;
    }
    auto it = tlvMap.find(TAG_AUTH_TYPE);
    if (it != tlvMap.end() && it->second == std::to_string(AuthVerifyType::RSA_3072_SHA512)) {
        hSession->verifyType = AuthVerifyType::RSA_3072_SHA512;
    }
    WRITE_LOG(LOG_INFO, "client auth type is %u for %s session", hSession->verifyType,
        Hdc::MaskSessionIdToString(hSession->sessionId).c_str());

    // Get server support features
    ParsePeerSupportFeatures(hSession, tlvMap);
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
    if (clsUSBServ != nullptr && hSession->connType == CONN_USB) {
        (reinterpret_cast<HdcDaemonUSB *>(clsUSBServ))->OnNewHandshakeOK(hSession->sessionId);
    }

    handshake.sessionId = 0;
    handshake.connectKey = "";
    // Get server capability
    GetServerCapability(hSession, handshake);
}

// host(  ) ---(TLS handshake client hello )--> hdcd(  ) step 1
// host(  ) <--(TLS handshake server hello )--- hdcd(  ) step 2
// host(ok) ---(TLS handshake change cipher)--> hdcd(  ) step 3
// host(ok) <--(TLS handshake change cipher)--- hdcd(ok) step 4
#ifdef HDC_SUPPORT_ENCRYPT_TCP
bool HdcDaemon::DaemonSSLHandshake(HSession hSession, const uint32_t channelId, SessionHandShake &handshake)
{
    uint8_t *payload = reinterpret_cast<uint8_t*>(handshake.buf.data());
    int payloadSize = static_cast<int>(handshake.buf.size());
    if (!hSession->classSSL) {
        WRITE_LOG(LOG_WARN, "DaemonSSLHandshake classSSL is nullptr");
        return false;
    }
    HdcSSLBase *hssl = static_cast<HdcSSLBase *>(hSession->classSSL);
    if (!hssl) {
        WRITE_LOG(LOG_WARN, "hssl is null");
        return false;
    }
    if (payloadSize > 0) {
        int retw = hssl->DoBIOWrite(payload, payloadSize);
        if (retw != payloadSize) {
            WRITE_LOG(LOG_WARN, "BIO_write failed, payloadsize is %d", payloadSize);
            return false;
        }
    }
    vector<uint8_t> buf;
    int ret = hssl->PerformHandshake(buf);
    if (ret == RET_SUCCESS) { // SSL handshake step 2 and step 4
        if (buf.size() == 0) { // no handshake data
            WRITE_LOG(LOG_WARN, "SSL PerformHandshake failed, buffer data size is 0");
            return false;
        }
        handshake.buf.assign(buf.begin(), buf.end());
        string bufString = SerialStruct::SerializeToString(handshake);
        Send(hSession->sessionId, channelId, CMD_KERNEL_HANDSHAKE,
             reinterpret_cast<uint8_t *>(const_cast<char *>(bufString.c_str())), bufString.size());
    }
    if (hssl->SetHandshakeLabel(hSession)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(SSL_HANDSHAKE_FINISHED_WAIT_TIME));
        UpdateSessionAuthOk(hSession->sessionId);
        SendAuthOkMsg(handshake, channelId, hSession);
        if (!hssl->ClearPsk()) {
            WRITE_LOG(LOG_WARN, "clear Pre Shared Key failed");
            ret = ERR_GENERIC;
        }
    }
    fill(buf.begin(), buf.end(), 0);
    return ret == RET_SUCCESS;
}
#endif

bool HdcDaemon::DaemonSessionHandshake(HSession hSession, const uint32_t channelId, uint8_t *payload, int payloadSize)
{
    StartTraceScope("HdcDaemon::DaemonSessionHandshake");
    // session handshake step2
    string s = string(reinterpret_cast<char *>(payload), payloadSize);
    SessionHandShake handshake;
    std::string sessionIdMaskStr = Hdc::MaskSessionIdToString(hSession->sessionId);
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
        if (GetSessionAuthStatus(handshake.sessionId) != AUTH_NONE) {
            WRITE_LOG(LOG_FATAL, "session %s is already, But now session is %s, refused!",
                Hdc::MaskSessionIdToString(handshake.sessionId).c_str(), sessionIdMaskStr.c_str());
            return false;
        }
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
    WRITE_LOG(LOG_INFO, "session %s handshakeOK send back CMD_KERNEL_HANDSHAKE", sessionIdMaskStr.c_str());
#endif
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
        case CMD_UNITY_EXECUTE_EX:
        case CMD_UNITY_REMOUNT:
        case CMD_UNITY_REBOOT:
        case CMD_UNITY_RUNMODE:
        case CMD_UNITY_HILOG:
        case CMD_UNITY_ROOTRUN:
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

bool HdcDaemon::CheckAuthStatus(HSession hSession, const uint32_t channelId, const uint16_t command)
{
    if (authEnable && (GetSessionAuthStatus(hSession->sessionId) != AUTH_OK) &&
        command != CMD_KERNEL_HANDSHAKE && command != CMD_KERNEL_CHANNEL_CLOSE && command != CMD_SSL_HANDSHAKE) {
        string authmsg = GetSessionAuthmsg(hSession->sessionId);
        WRITE_LOG(LOG_WARN, "session %s auth failed: %s for command %u",
                  Hdc::MaskSessionIdToString(hSession->sessionId).c_str(), authmsg.c_str(), command);
        if (!authmsg.empty()) {
            LogMsg(hSession->sessionId, channelId, MSG_FAIL, authmsg.c_str());
        }
        uint8_t count = 1;
        Send(hSession->sessionId, channelId, CMD_KERNEL_CHANNEL_CLOSE, &count, 1);
        return false;
    }
    return true;
}

static std::map<uint16_t, std::string> DaemonReportMap = {
    {HdcCommand::CMD_KERNEL_TARGET_CONNECT, CMDSTR_CONNECT_TARGET},
    {HdcCommand::CMD_SHELL_INIT, CMDSTR_SHELL},
    {HdcCommand::CMD_UNITY_RUNMODE, CMDSTR_TARGET_MODE},
    {HdcCommand::CMD_APP_CHECK, CMDSTR_APP_INSTALL},
    {HdcCommand::CMD_APP_UNINSTALL, CMDSTR_APP_UNINSTALL},
    {HdcCommand::CMD_FORWARD_INIT, CMDSTR_FORWARD_RPORT},
    {HdcCommand::CMD_UNITY_HILOG, CMDSTR_HILOG},
    {HdcCommand::CMD_JDWP_LIST, CMDSTR_LIST_JDWP},
    {HdcCommand::CMD_JDWP_TRACK, CMDSTR_TRACK_JDWP},
    {HdcCommand::CMD_UNITY_REMOUNT, CMDSTR_TARGET_MOUNT},
    {HdcCommand::CMD_UNITY_REBOOT, CMDSTR_TARGET_REBOOT},
    {HdcCommand::CMD_UNITY_BUGREPORT_INIT, CMDSTR_BUGREPORT},
};

static bool ReportCommandEvent(const uint16_t command, uint8_t *payload, const int payloadSize, bool isIntercepted)
{
#ifdef HDC_SUPPORT_REPORT_COMMAND_EVENT
    auto it = Hdc::DaemonReportMap.find(command);
    if (it != Hdc::DaemonReportMap.end() && it->second != "" &&
        !DelayedSingleton<CommandEventReport>::GetInstance()->ReportCommandEvent(
            it->second + " " + std::string(reinterpret_cast<char *>(payload), payloadSize),
            Base::GetCaller(), isIntercepted, it->second)) {
            WRITE_LOG(LOG_FATAL,
                "[E00C002]Execution intercepted due to inaccessibility of reporting command event.");
            return false;
        }
#endif
    return true;
}

static const std::unordered_map<uint16_t, STATISTIC_ITEM> commandToReportItem = {
    {CMD_UNITY_EXECUTE,       STATISTIC_ITEM::SHELL_COUNT},           // shell xxx
    {CMD_UNITY_EXECUTE_EX,    STATISTIC_ITEM::SHELL_COUNT},           // shell xxx
    {CMD_SHELL_INIT,          STATISTIC_ITEM::INTERACT_SHELL_COUNT},  // interact shell
    {CMD_APP_CHECK,           STATISTIC_ITEM::INSTALL_COUNT},         // install
    {CMD_APP_UNINSTALL,       STATISTIC_ITEM::UNINSTALL_COUNT},       // uninstall
    {CMD_FILE_CHECK,          STATISTIC_ITEM::FILE_SEND_COUNT},       // file send
    {CMD_FILE_INIT,           STATISTIC_ITEM::FILE_RECV_COUNT},       // file recv
    {CMD_FORWARD_CHECK,       STATISTIC_ITEM::FPORT_COUNT},           // fport
    {CMD_FORWARD_INIT,        STATISTIC_ITEM::RPORT_COUNT},           // rport
    {CMD_UNITY_HILOG,         STATISTIC_ITEM::HILOG_COUNT},           // hilog
    {CMD_JDWP_LIST,           STATISTIC_ITEM::JPID_COUNT},            // jpid
    {CMD_JDWP_TRACK,          STATISTIC_ITEM::TRACK_JPID_COUNT}       // track-jpid
};

void HdcDaemon::HandleCommandReport(const uint16_t command)

{
    auto it = commandToReportItem.find(command);
    if (it != commandToReportItem.end()) {
        HdcStatisticReporter::GetInstance().IncrCommandInfo(it->second);
    }
}

bool HdcDaemon::FetchCommand(HSession hSession, const uint32_t channelId, const uint16_t command, uint8_t *payload,
                             const int payloadSize)
{
    StartTraceScope("HdcDaemon::FetchCommand");
    bool isNotIntercepted = CheckControl(command);
    if (!ReportCommandEvent(command, payload, payloadSize, !isNotIntercepted)) {
        std::string msg = "[E00C002]Execution intercepted due to inaccessibility of reporting command event.";
        LogMsg(hSession->sessionId, channelId, MSG_FAIL, msg.c_str());
        return false;
    }
    bool ret = true;
    if (!CheckAuthStatus(hSession, channelId, command)) {
        return true;
    }
    if (command != CMD_UNITY_BUGREPORT_DATA && command != CMD_SHELL_DATA &&
        command != CMD_FORWARD_DATA && command != CMD_FILE_DATA && command != CMD_APP_DATA) {
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
            if (payloadSize >= 1 && *payload != 0) {
                --(*payload);
                Send(hSession->sessionId, channelId, CMD_KERNEL_CHANNEL_CLOSE, payload, 1);
            }
            ret = true;
            break;
        }
        case CMD_HEARTBEAT_MSG: {
            // heartbeat msg
            std::string str = hSession->heartbeat.HandleRecvHeartbeatMsg(payload, payloadSize);
            WRITE_LOG(LOG_INFO, "recv %s for session %s", str.c_str(),
                Hdc::MaskSessionIdToString(hSession->sessionId).c_str());
            break;
        }
        default:
            HandleCommandReport(command);
            ret = true;
            if (isNotIntercepted) {
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
        WRITE_LOG(LOG_FATAL, "RemoveInstanceTask taskClass is null channelId:%u", hTask->channelId);
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
    info.authtype = AUTH_NONE;

    mapAuthStatusMutex.lock();
    if (mapAuthStatus.count(sessionid) > 0) {
        info = mapAuthStatus[sessionid];
    }
    mapAuthStatusMutex.unlock();

    return info.authtype;
}
string HdcDaemon::GetSessionAuthToken(uint32_t sessionid)
{
    HdcDaemonAuthInfo info;

    mapAuthStatusMutex.lock();
    if (mapAuthStatus.count(sessionid) > 0) {
        info = mapAuthStatus[sessionid];
    }
    mapAuthStatusMutex.unlock();

    return info.token;
}
string HdcDaemon::GetSessionAuthPubkey(uint32_t sessionid)
{
    HdcDaemonAuthInfo info;

    mapAuthStatusMutex.lock();
    if (mapAuthStatus.count(sessionid) > 0) {
        info = mapAuthStatus[sessionid];
    }
    mapAuthStatusMutex.unlock();

    return info.pubkey;
}
string HdcDaemon::GetSessionAuthmsg(uint32_t sessionid)
{
    HdcDaemonAuthInfo info;

    mapAuthStatusMutex.lock();
    if (mapAuthStatus.count(sessionid) > 0) {
        info = mapAuthStatus[sessionid];
    }
    mapAuthStatusMutex.unlock();

    return info.authmsg;
}
void HdcDaemon::SendAuthOkMsg(SessionHandShake &handshake, uint32_t channelid,
                              HSession hSession, string msg, string daemonAuthResult)
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
        hSession->handshakeOK = true;
    } else {
        string emgmsg;
        Base::TlvAppend(emgmsg, TAG_EMGMSG, msg);
        Base::TlvAppend(emgmsg, TAG_DEVNAME, hostname);
        Base::TlvAppend(emgmsg, TAG_DAEOMN_AUTHSTATUS, daemonAuthResult);
        
        int connectValidationStatus = HdcValidation::GetConnectValidationParam();
        bool connectstatus = (connectValidationStatus == VALIDATION_HDC_DAEMON
            || connectValidationStatus == VALIDATION_HDC_HOST_AND_DAEMON);
        if (!connectstatus || hSession->supportConnValidation) {
            AddFeatureTagToEmgmsg(emgmsg);
            hSession->handshakeOK = true;
        }
        handshake.buf = emgmsg;
    }

    handshake.authType = AUTH_OK;
    string bufString = SerialStruct::SerializeToString(handshake);
    Send(hSession->sessionId, channelid, CMD_KERNEL_HANDSHAKE,
            reinterpret_cast<uint8_t *>(const_cast<char *>(bufString.c_str())), bufString.size());
    uint8_t count = 1;
    Send(hSession->sessionId, channelid, CMD_KERNEL_CHANNEL_CLOSE, &count, 1);
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
void HdcDaemon::SendAuthMsg(SessionHandShake &handshake, const uint32_t channelId,
    HSession &hSession, string pubkey)
{
#ifdef HDC_SUPPORT_ENCRYPT_TCP
    if (hSession->connType == CONN_TCP && hSession->supportEncrypt) {
        SendAuthEncryptPsk(handshake, channelId, hSession, pubkey);
        return;
    }
#endif
    SendAuthSignMsg(handshake, channelId, hSession->sessionId, pubkey, hSession->tokenRSA);
}
#ifdef HDC_SUPPORT_ENCRYPT_TCP
void HdcDaemon::SendAuthEncryptPsk(SessionHandShake &handshake, const uint32_t channelId,
    HSession &hSession, string pubkey)
{
    UpdateSessionAuthPubkey(hSession->sessionId, pubkey);
    handshake.authType = AUTH_SSL_TLS_PSK;
    if (!hSession->classSSL) {
        SSLInfoPtr hSSLInfo = new (std::nothrow) HdcSSLInfo();
        if (!hSSLInfo) {
            WRITE_LOG(LOG_FATAL, "SendAuthEncryptPsk new HdcSSLInfo failed");
            return;
        }
        HdcSSLBase::SetSSLInfo(hSSLInfo, hSession);
        hSession->classSSL = new (std::nothrow) HdcDaemonSSL(hSSLInfo); // long lifetime with session.
        delete hSSLInfo;
        if (!hSession->classSSL) {
            WRITE_LOG(LOG_FATAL, "SendAuthEncryptPsk new HdcDaemonSSL failed");
            return;
        }
    }
    HdcSSLBase *hssl = static_cast<HdcSSLBase *>(hSession->classSSL);
    if (!hssl) {
        WRITE_LOG(LOG_WARN, "hssl is null");
        return;
    }
    if (!hssl->GenPsk()) {
        WRITE_LOG(LOG_WARN, "gen psk failed");
        return;
    }
    std::unique_ptr<unsigned char[]> payload(std::make_unique<unsigned char[]>(BUF_SIZE_DEFAULT2));
    int payloadSize = hssl->GetPskEncrypt(payload.get(), BUF_SIZE_DEFAULT2, pubkey);
    if (payloadSize <= 0) {
        WRITE_LOG(LOG_WARN, "RsaPubkeyEncrpt failed");
        return;
    }

    handshake.buf = string(reinterpret_cast<const char*>(payload.get()), payloadSize);
    string bufString = SerialStruct::SerializeToString(handshake);
    Send(hSession->sessionId, channelId, CMD_KERNEL_HANDSHAKE,
         reinterpret_cast<uint8_t *>(const_cast<char *>(bufString.c_str())), bufString.size());
    hssl->InitSSL();
}
#endif
void HdcDaemon::HandleAuthFailed(SessionHandShake &handshake, uint32_t channelid, HSession hSession, string msg)
{
    SendAuthOkMsg(handshake, channelid, hSession, msg, DAEOMN_UNAUTHORIZED);
    LogMsg(hSession->sessionId, channelid, MSG_FAIL, msg.c_str());
    UpdateSessionAuthmsg(hSession->sessionId, msg);
}
void HdcDaemon::AuthRejectLowClient(SessionHandShake &handshake, uint32_t channelid, HSession hSession)
{
    string msg = "[E000001]:The sdk hdc.exe version is too low, please upgrade to the latest version.";
    HandleAuthFailed(handshake, channelid, hSession, msg);
}
void HdcDaemon::AddFeatureTagToEmgmsg(string &emgmsg)
{
    Base::TlvAppend(emgmsg, TAG_FEATURE_SHELL_OPT, "enable");
    // told server, we support features
    Base::TlvAppend(emgmsg, TAG_SUPPORT_FEATURE, Base::FeatureToString(Base::GetSupportFeature()));
}
void HdcDaemon::AuthRejectNotSupportConnValidation(SessionHandShake &handshake, uint32_t channelid, HSession hSession)
{
    string msg = "[E000006]:The sdk hdc.exe is not allowed to be debugged by enterprise control.";
    HandleAuthFailed(handshake, channelid, hSession, msg);
}
}  // namespace Hdc
