/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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
#include <sys/socket.h>
#include "credential_base.h"
#include "credential_message.h"
#include "hdc_huks.h"
#include "hdc_subscriber.h"
#include "password.h"

using namespace Hdc;
using namespace HdcCredentialBase;

Hdc::HdcHuks hdcHuks(HDC_PRIVATE_KEY_FILE_PWD_KEY_ALIAS);
Hdc::HdcPassword pwd(HDC_PRIVATE_KEY_FILE_PWD_KEY_ALIAS);

std::string BytetoHex(const uint8_t* byteDate, size_t length)
{
    uint8_t tmp;
    std::string encryptPwd;

    for (size_t i = 0; i < length; i++) {
        tmp = byteDate[i];
        encryptPwd.push_back(pwd.GetHexChar(tmp >> 4)); // 4 get high 4 bits
        encryptPwd.push_back(pwd.GetHexChar(tmp & 0x0F));
    }
    return encryptPwd;
}

bool ResetPwdKey(void)
{
    return hdcHuks.ResetHuksKey();
}

std::string CredentialEncryptPwd(const std::string& messageStr)
{
    if (messageStr.size() != PASSWORD_LENGTH) {
        WRITE_LOG(LOG_FATAL, "Invalid input length: expected %d, got %zu", PASSWORD_LENGTH, messageStr.size());
        return "";
    }
    std::vector<uint8_t> encryptData;

    bool encryptResult = hdcHuks.AesGcmEncrypt(reinterpret_cast<const uint8_t*>(messageStr.c_str()),
        PASSWORD_LENGTH, encryptData);
    if (!encryptResult) {
        WRITE_LOG(LOG_FATAL, "CredentialEncryptPwd: AES GCM encryption failed.");
        return "";
    }

    return std::string(reinterpret_cast<const char*>(encryptData.data()), encryptData.size());
}

std::string EncryptPwd(const std::string& messageStr)
{
    if (!ResetPwdKey()) {
        WRITE_LOG(LOG_FATAL, "EncryptPwd: ResetPwdKey failed.");
        return "";
    }

    std::string encryptPwd = CredentialEncryptPwd(messageStr);
    if (encryptPwd.empty()) {
        WRITE_LOG(LOG_FATAL, "EncryptPwd: CredentialEncryptPwd failed.");
        return "";
    }

    return encryptPwd;
}

std::string DecryptPwd(const std::string& messageStr)
{
    uint8_t pwd[PASSWORD_LENGTH] = {0};
    std::pair<uint8_t*, int> decryptPwd = hdcHuks.AesGcmDecrypt(messageStr);
    if (decryptPwd.first == nullptr) {
        WRITE_LOG(LOG_FATAL, "AesGcmDecrypt failed.");
        return "";
    }

    do {
        if (decryptPwd.second != PASSWORD_LENGTH) {
            WRITE_LOG(LOG_FATAL, "Invalid pwd len %d", decryptPwd.second);
            break;
        }
        int ret = memcpy_s(pwd, PASSWORD_LENGTH, decryptPwd.first, decryptPwd.second);
        if (ret != EOK) {
            WRITE_LOG(LOG_FATAL, "Copy failed.ret is %d", ret);
            break;
        }
    } while (0);

    memset_s(decryptPwd.first, decryptPwd.second, 0, decryptPwd.second);
    delete[] decryptPwd.first;

    std::string pwdStr(reinterpret_cast<const char*>(pwd), PASSWORD_LENGTH);
    memset_s(pwd, PASSWORD_LENGTH, 0, PASSWORD_LENGTH);

    return pwdStr;
}

std::string ParseAndProcessMessageStr(const std::string& messageStr)
{
    CredentialMessage messageStruct(messageStr);
    if (messageStruct.GetMessageBody().empty() ||
        messageStruct.GetMessageVersion() != METHOD_VERSION_V1) {
        WRITE_LOG(LOG_FATAL, "Invalid message structure or version not v1.");
        return "";
    }
    std::string processMessageValue;
    switch (messageStruct.GetMessageMethodType()) {
        case METHOD_ENCRYPT: {
            processMessageValue = EncryptPwd(messageStruct.GetMessageBody());
            break;
        }
        case METHOD_DECRYPT: {
            processMessageValue = DecryptPwd(messageStruct.GetMessageBody());
            break;
        }
        default: {
            WRITE_LOG(LOG_FATAL, "Unsupported message method type.");
            return "";
        }
    }

    messageStruct.SetMessageBody(processMessageValue);

    return messageStruct.Construct();
}

int CreateAndBindSocket(const std::string& socketPath)
{
    if (access(socketPath.c_str(), F_OK) == 0) {
        if (remove(socketPath.c_str()) < 0) {
            WRITE_LOG(LOG_FATAL, "Failed to remove existing socket file, message: %s.", strerror(errno));
            return -1;
        }
    }

    int sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to create socket.");
        return -1;
    }

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    size_t maxPathLen = sizeof(addr.sun_path) - 1;
    size_t pathLen = socketPath.size();
    if (pathLen > sizeof(addr.sun_path) - 1) {
        WRITE_LOG(LOG_FATAL, "Socket path too long.");
        close(sockfd);
        return -1;
    }
    memcpy_s(addr.sun_path, maxPathLen, socketPath.c_str(), pathLen);

    if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to bind socket, message: %s.", strerror(errno));
        close(sockfd);
        return -1;
    }

    if (chmod(addr.sun_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) != 0) {
        WRITE_LOG(LOG_FATAL, "Failed to chmod socket file, message: %s.", strerror(errno));
        close(sockfd);
        return -1;
    }
    return sockfd;
}

std::string CredentialUsage()
{
    std::string ret = "";
    ret = "\n                         Harmony device connector (HDC) credential process \n\n"
          "Usage: hdc_credential [options]...\n"
          "\n"
          "general options:\n"
          " -h                            - Print help\n"
          " -v                            - Print version information\n";
    return ret;
}

std::string CredentialVersion()
{
    const uint8_t a = 'a';
    uint8_t major = (CREDENTIAL_VERSION_NUMBER >> 28) & 0xff;
    uint8_t minor = (CREDENTIAL_VERSION_NUMBER << 4 >> 24) & 0xff;
    uint8_t version = (CREDENTIAL_VERSION_NUMBER << 12 >> 24) & 0xff;
    uint8_t fix = (CREDENTIAL_VERSION_NUMBER << 20 >> 28) & 0xff;  // max 16, tail is p
    std::string ver = StringFormat("%x.%x.%x%c", major, minor, version, a + fix);
    return "Ver: " + ver;
}

bool SplitCommandToArgs(int argc, const char **argv)
{
    if (argc == CMD_ARG1_COUNT) {
        if (!strcmp(argv[1], "-h")) {
            std::string usage = CredentialUsage();
            fprintf(stderr, "%s", usage.c_str());
            return false;
        } else if (!strcmp(argv[1], "-v")) {
            std::string ver = CredentialVersion();
            fprintf(stderr, "%s\n", ver.c_str());
            return false;
        }
    }
    if (argc != 1) {
        fprintf(stderr, "Invalid input parameters, please recheck.\n");
        std::string usage = CredentialUsage();
        fprintf(stderr, "%s\n", usage.c_str());
        return false;
    }
    return true;
}
int main(int argc, const char *argv[])
{
    if (!SplitCommandToArgs(argc, argv)) {
        return 0;
    }
    if (HdcAccountSubscriberMonitor() != 0) {
        WRITE_LOG(LOG_FATAL, "HdcAccountSubscriberMonitor failed");
        return 0;
    }
    // fresh all accounts path when process restart.
    FreshAccountsPath();

    int sockfd = CreateAndBindSocket(HDC_CREDENTIAL_SOCKET_REAL_PATH.c_str());
    if (sockfd < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to create and bind socket.");
        return -1;
    }
    if (listen(sockfd, SOCKET_CLIENT_NUMS) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to listen on socket.");
        close(sockfd);
        return -1;
    }
    WRITE_LOG(LOG_INFO, "Listening on socket: %s", HDC_CREDENTIAL_SOCKET_REAL_PATH.c_str());
    bool running = true;
    while (running) {
        int connfd = accept(sockfd, nullptr, nullptr);
        if (connfd < 0) {
            WRITE_LOG(LOG_FATAL, "Failed to accept connection!");
            continue;
        }

        char buffer[MESSAGE_STR_MAX_LEN] = {0};
        ssize_t bytesRead = read(connfd, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) {
            WRITE_LOG(LOG_FATAL, "Error: Failed to read from socket.");
            close(connfd);
            continue;
        }
        std::string sendBuf = ParseAndProcessMessageStr(std::string(buffer, bytesRead));
        if (sendBuf.empty()) {
            WRITE_LOG(LOG_FATAL, "Error: Processed message is empty.");
            close(connfd);
            continue;
        }

        size_t bytesSend = write(connfd, sendBuf.c_str(), sendBuf.size());
        if (bytesSend != sendBuf.size()) {
            WRITE_LOG(LOG_FATAL, "Failed to send message.");
            close(connfd);
            continue;
        }
        memset_s(buffer, sizeof(buffer), 0, sizeof(buffer)); // Clear the buffer
        close(connfd);
    } // Keep the server running indefinitely
    WRITE_LOG(LOG_FATAL, "hdc_credential stopped.");
    close(sockfd);
    unlink(HDC_CREDENTIAL_SOCKET_REAL_PATH.c_str());
    return 0;
}