/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

#include "bridge.h"
#include "log.h"
#include "sys_para.h"
#include "base.h"

#include <unistd.h>
#include <fcntl.h>
namespace Hdc {
static uint8_t *g_bridgeReadBuf = nullptr;
static constexpr char BRIDGE_FILE_PATH[20] = "/dev/express_bridge";

HdcBridge::HdcBridge()
{
    string strBridgePort;
    GetDevItem("persist.hdc.port", strBridgePort);
    WRITE_LOG(LOG_INFO, "strBridgePort:%s", strBridgePort.c_str());
    bridgeListenPort = atoi(strBridgePort.c_str());
    if (bridgeListenPort <= 0) {
        bridgeListenPort = 0;
    }
    WRITE_LOG(LOG_INFO, "bridgeListenPort:%d", bridgeListenPort);
}

HdcBridge::~HdcBridge()
{
}

int HdcBridge::StartListen()
{
    bridgeFd = open(BRIDGE_FILE_PATH, O_RDWR);
    WRITE_LOG(LOG_INFO, "StartListen bridgeFd:%d", bridgeFd);
    if (bridgeFd <= 0) {
        WRITE_LOG(LOG_FATAL, "SetBridgeListen open failed");
        return -1;
    }
    int ret = ioctl(bridgeFd, IOC_BIND, static_cast<unsigned long>(bridgeListenPort));
    WRITE_LOG(LOG_INFO, "StartListen ioctl ret:%d", ret);
    if (ret < 0) {
        WRITE_LOG(LOG_FATAL, "SetBridgeListen IOC_BIND failed");
        return -1;
    }
    return bridgeFd;
}

int HdcBridge::HandleClient(int socketFd)
{
    int newClientFd = open(BRIDGE_FILE_PATH, O_RDWR);
    WRITE_LOG(LOG_INFO, "HandleClient newClientFd:%d", newClientFd);
    if (newClientFd < 0) {
        WRITE_LOG(LOG_FATAL, "Unable to open new bridge connection err %d", errno);
        return -1;
    }
    errno = 0;
    int ret = ioctl(newClientFd, IOC_CONNECT, static_cast<unsigned long>(socketFd));
    if (ret < 0) {
        WRITE_LOG(LOG_FATAL, "Unable to ioctl new bridge err %d", errno);
        close(newClientFd);
        return -1;
    }
    return newClientFd;
}

int HdcBridge::ReadPipeFd(int fd, char* buf, int size)
{
    WRITE_LOG(LOG_INFO, "ReadPipeFd start");
    return read(fd, buf, size);
}

PersistBuffer HdcBridge::ReadClient(int fd, int size)
{
    if (g_bridgeReadBuf == nullptr) {
        WRITE_LOG(LOG_DEBUG, "remalloc g_bridgeReadBuf");
        g_bridgeReadBuf = new uint8_t[MAX_SIZE_IOBUF];
    }
    int readSize = read(fd, g_bridgeReadBuf, size);
    return PersistBuffer{reinterpret_cast<char *>(g_bridgeReadBuf), static_cast<uint64_t>(readSize)};
}

int HdcBridge::WriteClient(int fd, SerializedBuffer buf)
{
    uint8_t* ptr = reinterpret_cast<uint8_t *>(buf.ptr);
    size_t size = static_cast<size_t>(buf.size);
    int cnt = size;
    constexpr int intrmax = 1000;
    int intrcnt = 0;
    while (cnt > 0) {
        int rc = write(fd, ptr, cnt);
        if (rc < 0) {
            int err = errno;
            if (err != EINTR && err != EAGAIN) {
                WRITE_LOG(LOG_FATAL, "WriteClient fd:%d send rc:%d err:%d", fd, rc, err);
                cnt = -1;
                break;
            }
            if (++intrcnt > intrmax) {
                WRITE_LOG(LOG_WARN, "WriteClient fd:%d send interrupt err:%d", fd, err);
                intrcnt = 0;
            }
            continue;
        }
        ptr += rc;
        cnt -= rc;
    }
    return cnt == 0 ? size : cnt;
}

void HdcBridge::Stop()
{
    if (bridgeFd > 0) {
        close(bridgeFd);
        bridgeFd = -1;
    }

    if (g_bridgeReadBuf != nullptr) {
        delete[] g_bridgeReadBuf;
        g_bridgeReadBuf = nullptr;
    }
}

extern "C" void* InitBridge()
{
    HdcBridge* instance = new HdcBridge();
    return instance;
}

extern "C" int StartListen(void* ptr)
{
    HdcBridge* bridge = (HdcBridge*)ptr;
    if (bridge == nullptr) {
        return -1;
    }
    return bridge->StartListen();
}

extern "C" int AcceptServerSocketFd(void* ptr, int pipeFd)
{
    WRITE_LOG(LOG_INFO, "AcceptServerSocketFd start, pipeFd:%d", pipeFd);
    HdcBridge* bridge = (HdcBridge*)ptr;
    if (bridge == nullptr) {
        return -1;
    }
    char socketFdBuf[4] = { 0 };
    int ret = bridge->ReadPipeFd(pipeFd, socketFdBuf, 4);
    WRITE_LOG(LOG_INFO, "AcceptServerSocketFd get socketfd buf size:%d", ret);
    if (ret < 0) {
        WRITE_LOG(LOG_INFO, "AcceptServerSocketFd get socket fd fail");
        return -1;
    }
    int socketFd = *reinterpret_cast<int*>(socketFdBuf);
    WRITE_LOG(LOG_INFO, "AcceptServerSocketFd get socketfd:%d", socketFd);
    return socketFd;
}

extern "C" int InitClientFd(void* ptr, int socketFd)
{
    HdcBridge* bridge = (HdcBridge*)ptr;
    if (bridge == nullptr) {
        return -1;
    }
    return bridge->HandleClient(socketFd);
}

extern "C" PersistBuffer ReadClient(void* ptr, int fd, int size)
{
    HdcBridge* bridge = (HdcBridge*)ptr;
    if (bridge == nullptr) {
        return PersistBuffer{reinterpret_cast<char *>(0), static_cast<uint64_t>(0)};
    }
    return bridge->ReadClient(fd, size);
}

extern "C" int WriteClient(void* ptr, int fd, SerializedBuffer buf)
{
    HdcBridge* bridge = (HdcBridge*)ptr;
    if (bridge == nullptr) {
        return -1;
    }
    return bridge->WriteClient(fd, buf);
}

extern "C" int Stop(void* ptr)
{
    HdcBridge* bridge = (HdcBridge*)ptr;
    if (bridge == nullptr) {
        return -1;
    }
    bridge->Stop();
    return 0;
}
}

