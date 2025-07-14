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
#include "credential_message.h"
#include "hdc_subscriber.h"

using namespace Hdc;

Hdc::HdcHuks hdc_huks(HDC_PRIVATE_KEY_FILE_PWD_KEY_ALIAS);

bool ResetPwdKey(void)
{
    return hdc_huks.ResetHuksKey();
}

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

std::string CredentialEncryptPwd(const std::string& messageStr)
{
    std::vector<uint8_t> encryptData;
    const char* rawCharData = messageStr.c_str();
    const uint8_t* uint8MessageStr = reinterpret_cast<const uint8_t*>(rawCharData);

    bool encryptResult = hdc_huks.AesGcmEncrypt(uint8MessageStr, PASSWORD_LENGTH, encryptData);
    if (!encryptResult) {
        WRITE_LOG(LOG_FATAL, "CredentialEncryptPwd: AES GCM encryption failed.");
        return "";
    }

    return std::string(reinterpret_cast<const char*>(encryptData.data()), encryptData.size());
}

std::pair<std::string, size_t> EncryptPwd(const std::string& messageStr)
{
    if (!ResetPwdKey()) {
        WRITE_LOG(LOG_FATAL, "EncryptPwd: ResetPwdKey failed.");
        return std::make_pair(std::string(), 0);
    }

    std::string encryptPwd = CredentialEncryptPwd(messageStr);
    if (encryptPwd.empty()) {
        WRITE_LOG(LOG_FATAL, "EncryptPwd: CredentialEncryptPwd failed.");
        return std::make_pair(std::string(), 0);
    }

    return std::make_pair(encryptPwd, encryptPwd.size());
}

std::pair<std::string, size_t> DecryptPwd(const std::string& messageStr)
{
    uint8_t pwd[PASSWORD_LENGTH] = {0};
    std::pair<uint8_t*, int> decryptPwd = hdc_huks.AesGcmDecrypt(messageStr);
    if (decryptPwd.first == nullptr) {
        WRITE_LOG(LOG_FATAL, "AesGcmDecrypt failed.");
        return std::make_pair(std::string(), 0);
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

    std::string pwd_str(reinterpret_cast<const char*>(pwd), PASSWORD_LENGTH);
    memset_s(pwd, PASSWORD_LENGTH, 0, PASSWORD_LENGTH);

    return std::make_pair(pwd_str, pwd_str.size());
}

std::string ParseAndProcessMessageStr(const std::string& messageStr)
{
    CredentialMessage messageStruct(messageStr);
    if (messageStruct.GetMessageBody().empty() ||
        messageStruct.GetMessageVersion() != METHOD_VERSION_V1) {
        WRITE_LOG(LOG_FATAL, "Invalid message structure or version not v1.");
        return "";
    }
    std::pair<std::string, size_t> processMessageValue;
    switch (messageStruct.GetMessageMethodType()) {
        case METHOD_ENCRYPT: {
            processMessageValue = EncryptPwd(messageStruct.GetMessageBody());
            break;
        }
        case METHOD_DECRYPT: {
            processMessageValue = DecryptPwd(messageStruct.GetMessageBody());
            break;
        }
        default:
            break;
    }

    messageStruct.SetMessageBody(processMessageValue.first);

    return messageStruct.Construct();
}

int create_and_bind_socket(const std::string& socketPath)
{
    int sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to create socket.");
        return -1;
    }

    struct sockaddr_un addr;
    memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    size_t buf_size = socketPath.length() + 1;
    memcpy_s(addr.sun_path, buf_size, socketPath.c_str(), buf_size);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &timeout, sizeof(timeout)) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to set socket options, message: %s.", strerror(errno));
        close(sockfd);
        return -1;
    }

    if (access(socketPath.c_str(), F_OK) == 0) {
        if (remove(socketPath.c_str()) < 0) {
            WRITE_LOG(LOG_FATAL, "Failed to remove existing socket file, message: %s.", strerror(errno));
            return -1;
        }
    }

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
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

string CredentialUsage()
{
    string ret = "";
    ret = "\n                         Harmony device connector (HDC) credential process \n\n"
          "Usage: hdc_credential [options]...\n"
          "\n"
          "general options:\n"
          " -h                            - Print help\n"
          " -v                            - Print version information\n";
    return ret;
}

string CredentialVersion()
{
    const uint8_t a = 'a';
    uint8_t major = (CREDENTIAL_VERSION_NUMBER >> 28) & 0xff;
    uint8_t minor = (CREDENTIAL_VERSION_NUMBER << 4 >> 24) & 0xff;
    uint8_t version = (CREDENTIAL_VERSION_NUMBER << 12 >> 24) & 0xff;
    uint8_t fix = (CREDENTIAL_VERSION_NUMBER << 20 >> 28) & 0xff;  // max 16, tail is p
    string ver = Base::StringFormat("%x.%x.%x%c", major, minor, version, a + fix);
    return "Ver: " + ver;
}

bool SplitCommandToArgs(int argc, const char **argv)
{
    if (argc == CMD_ARG1_COUNT) {
        if (!strcmp(argv[1], "-h")) {
            string usage = CredentialUsage();
            fprintf(stderr, "%s", usage.c_str());
            return false;
        } else if (!strcmp(argv[1], "-v")) {
            string ver = CredentialVersion();
            fprintf(stderr, "%s\n", ver.c_str());
            return false;
        }
    }
    if (argc != 1) {
        fprintf(stderr, "Invalid input parameters, please recheck.\n");
        string usage = CredentialUsage();
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
    HdcAccountSubscriberMonitor();
    int sockfd = create_and_bind_socket(HDC_CREDENTIAL_SOCKET_REAL_PATH);
    if (sockfd < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to create and bind socket.");
        return -1;
    }
    if (listen(sockfd, SOCKET_CLIENT_NUMS) < 0) {
        WRITE_LOG(LOG_FATAL, "Failed to listen on socket.");
        close(sockfd);
        return -1;
    }
    WRITE_LOG(LOG_INFO, "Listening on socket: %s", HDC_CREDENTIAL_SOCKET_REAL_PATH);
    bool running = true;
    while (running) {
        int connfd = accept(sockfd, nullptr, nullptr);
        if (connfd < 0) {
            WRITE_LOG(LOG_FATAL, "Failed to accept connection!");
            continue;
        }

        char buffer[MESSAGE_STR_MAX_LEN] = {0};
        ssize_t bytesRead = read(connfd, buffer, sizeof(buffer) - 1);
        if (bytesRead < 0) {
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
        }
        memset_s(buffer, sizeof(buffer), 0, sizeof(buffer)); // Clear the buffer
        close(connfd);
    } // Keep the server running indefinitely
    close(sockfd);
    unlink(HDC_CREDENTIAL_SOCKET_REAL_PATH);
    return 0;
}