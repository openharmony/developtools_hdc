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
#include "file_descriptor.h"

#if !defined(HDC_HOST) || defined(HOST_OHOS)
#include <sys/epoll.h>
#endif

#include "memory_pool.h"

namespace {
static constexpr int SECONDS_TIMEOUT = 5;

// 120% of max buf size, use stable size to avoid no buf
static const int IO_THREAD_READ_MAX = Hdc::Base::GetMaxBufSizeStable() * 1.2;

#if !defined(HDC_HOST) || defined(HOST_OHOS)
static constexpr int EPOLL_SIZE = 1;
int WaitIo(int fd, void* events)
{
    return epoll_wait(fd, static_cast<struct epoll_event*>(events), EPOLL_SIZE, SECONDS_TIMEOUT * Hdc::TIME_BASE);
}
#else
int WaitIo(int fd, void*)
{
    struct timeval timeout;
    timeout.tv_sec = SECONDS_TIMEOUT;
    timeout.tv_usec = 0;
    fd_set rset;
    FD_ZERO(&rset);
#ifdef _WIN32
    FD_SET(static_cast<SOCKET>(fd), &rset);
#else
    FD_SET(fd, &rset);
#endif
    return select(fd + 1, &rset, nullptr, nullptr, &timeout);
}
#endif

void CloseIoFd(int epollFd, int ioFd)
{
#if !defined(HDC_HOST) || defined(HOST_OHOS)
    if ((ioFd > 0) && (epoll_ctl(epollFd, EPOLL_CTL_DEL, ioFd, nullptr) == -1)) {
        Hdc::WRITE_LOG(Hdc::LOG_INFO, "EPOLL_CTL_DEL fail fd:%d epollFd:%d errno:%d",
            ioFd, epollFd, errno);
    }
    close(epollFd);
#endif
}
} // namespace

namespace Hdc {

FileIoThread::FileIoThread(HdcFileDescriptor* ptr)
{
    descriptor = ptr;
}

FileIoThread::~FileIoThread()
{
    if (buf != nullptr) {
        MemoryPool::Instance().Deallocate(buf);
    }
}

void FileIoThread::Run()
{
    if (!Malloc() || descriptor == nullptr) {
        return;
    }

#if !defined(HDC_HOST) || defined(HOST_OHOS)
    int epollFd = epoll_create(EPOLL_SIZE);
    struct epoll_event ev;
    struct epoll_event events[EPOLL_SIZE];
    ev.data.fd = descriptor->fdIO;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, descriptor->fdIO, &ev);
#else
    int epollFd = descriptor->fdIO;
    void* events = nullptr;
#endif
    while (true) {
        if (!ReadData(epollFd, events)) {
            break;
        }
    }

    CloseIoFd(epollFd, descriptor->fdIO);

    --descriptor->refIO;
    descriptor->workContinue = false;
    descriptor->callbackFinish(descriptor->callerContext, fetalFinish, STRING_EMPTY);
}

bool FileIoThread::ReadData(int epollFd, void* events)
{
    if (!PrepareBuf()) {
        return false;
    }

    int rc = WaitIo(epollFd, events);
    if (rc < 0) {
        auto err = errno;
        WRITE_LOG(LOG_FATAL, "FileIOOnThread select or epoll_wait fdIO:%d error:%d",
            descriptor->fdIO, err);
        return err == EINTR || err == EAGAIN;
    } else if (rc == 0) {
        WRITE_LOG(LOG_WARN, "FileIOOnThread select rc = 0, timeout.");
        return true;
    }
    ssize_t nBytes = 0;
#if !defined(HDC_HOST) || defined(HOST_OHOS)
    uint32_t event = static_cast<struct epoll_event*>(events)->events;
    if ((event & EPOLLIN) && (descriptor->fdIO > 0)) {
        nBytes = read(descriptor->fdIO, buf, bufSize);
    }
    if ((event & EPOLLERR) || (event & EPOLLHUP) || (event & EPOLLRDHUP)) {
        fetalFinish = true;
        if ((nBytes > 0) && !descriptor->callbackRead(descriptor->callerContext, buf, nBytes)) {
            WRITE_LOG(LOG_WARN, "FileIOOnThread fdIO:%d callbackRead false", descriptor->fdIO);
        }
        return false;
    }
#else
    if (descriptor->fdIO > 0) {
        nBytes = read(descriptor->fdIO, buf, bufSize);
    }
#endif
    if (nBytes < 0 && (errno == EINTR || errno == EAGAIN)) {
        WRITE_LOG(LOG_WARN, "FileIOOnThread fdIO:%d read interrupt", descriptor->fdIO);
        return true;
    }
    if (nBytes > 0) {
        if (!descriptor->callbackRead(descriptor->callerContext, buf, nBytes)) {
            WRITE_LOG(LOG_WARN, "FileIOOnThread fdIO:%d callbackRead false", descriptor->fdIO);
            return false;
        }
        return true;
    } else {
        WRITE_LOG(LOG_INFO, "FileIOOnThread fd:%d nBytes:%d errno:%d",
            descriptor->fdIO, nBytes, errno);
        fetalFinish = true;
        return false;
    }
}

bool FileIoThread::Malloc()
{
    buf = static_cast<uint8_t*>(MemoryPool::Instance().Allocate(IO_THREAD_READ_MAX));
    if (buf == nullptr) {
        descriptor->callbackFinish(descriptor->callerContext, true, "Memory alloc failed");
        return false;
    }
    bufSize = IO_THREAD_READ_MAX;
    return true;
}

bool FileIoThread::PrepareBuf()
{
    if (descriptor->workContinue == false) {
        WRITE_LOG(LOG_INFO, "FileIOOnThread fdIO:%d workContinue false", descriptor->fdIO);
        return false;
    }

    if (memset_s(buf, bufSize, 0, bufSize) != EOK) {
        WRITE_LOG(LOG_FATAL, "FileIOOnThread buf memset_s fail.");
        return false;
    }

    return true;
}

HdcFileDescriptor::HdcFileDescriptor(uv_loop_t *loopIn, int fdToRead, void *callerContextIn,
                                     CallBackWhenRead callbackReadIn, CmdResultCallback callbackFinishIn,
                                     bool interactiveShell)
{
    loop = loopIn;
    workContinue = true;
    callbackFinish = callbackFinishIn;
    callbackRead = callbackReadIn;
    fdIO = fdToRead;
    refIO = 0;
    isInteractive = interactiveShell;
    callerContext = callerContextIn;
    if (isInteractive) {
        std::thread([this]() {
            HdcFileDescriptor::IOWriteThread(this);
        }).detach();
    }
}

HdcFileDescriptor::~HdcFileDescriptor()
{
    workContinue = false;
    if (isInteractive) {
        NotifyWrite();
        uv_sleep(MILL_SECONDS);
    }
}

bool HdcFileDescriptor::ReadyForRelease()
{
    return refIO == 0;
}

// just tryCloseFdIo = true, callback will be effect
void HdcFileDescriptor::StopWorkOnThread(bool tryCloseFdIo, std::function<void()> closeFdCallback)
{
    workContinue = false;
    if (isInteractive) {
        NotifyWrite();
    }

    callbackCloseFd = closeFdCallback;
    if (tryCloseFdIo && refIO > 0) {
        if (callbackCloseFd != nullptr) {
            callbackCloseFd();
        }
    }
}

int HdcFileDescriptor::LoopReadOnThread()
{
    ++refIO;
    std::thread([this]() {
        FileIoThread thread(this);
        thread.Run();
    }).detach();
    return 0;
}

bool HdcFileDescriptor::StartWorkOnThread()
{
    if (LoopReadOnThread() < 0) {
        return false;
    }
    return true;
}

int HdcFileDescriptor::Write(uint8_t *data, int size)
{
    if (size > static_cast<int>(HDC_BUF_MAX_BYTES - 1)) {
        size = static_cast<int>(HDC_BUF_MAX_BYTES - 1);
    }
    if (size <= 0) {
        WRITE_LOG(LOG_WARN, "Write failed, size:%d", size);
        return -1;
    }
    auto buf = new(std::nothrow) uint8_t[size];
    if (!buf) {
        return -1;
    }
    if (memcpy_s(buf, size, data, size) != EOK) {
        delete[] buf;
        return -1;
    }
    return WriteWithMem(buf, size);
}

// Data's memory must be Malloc, and the callback FREE after this function is completed
int HdcFileDescriptor::WriteWithMem(uint8_t *data, int size)
{
#ifdef CONFIG_USE_JEMALLOC_DFX_INIF
    mallopt(M_DELAYED_FREE, M_DELAYED_FREE_DISABLE);
    mallopt(M_SET_THREAD_CACHE, M_THREAD_CACHE_DISABLE);
#endif
    auto contextIO = new(std::nothrow) CtxFileIO();
    if (!contextIO) {
        delete[] data;
        WRITE_LOG(LOG_FATAL, "Memory alloc failed");
        callbackFinish(callerContext, true, "Memory alloc failed");
        return -1;
    }
    contextIO->bufIO = data;
    contextIO->size = static_cast<size_t>(size);
    contextIO->thisClass = this;
    PushWrite(contextIO);
    NotifyWrite();
    return size;
}

void HdcFileDescriptor::IOWriteThread(void *object)
{
    HdcFileDescriptor *hfd = reinterpret_cast<HdcFileDescriptor *>(object);
    while (hfd->workContinue) {
        hfd->HandleWrite();
        hfd->WaitWrite();
    }
}

void HdcFileDescriptor::PushWrite(CtxFileIO *cfio)
{
    std::unique_lock<std::mutex> lock(writeMutex);
    writeQueue.push(cfio);
}

CtxFileIO *HdcFileDescriptor::PopWrite()
{
    std::unique_lock<std::mutex> lock(writeMutex);
    CtxFileIO *cfio = nullptr;
    if (!writeQueue.empty()) {
        cfio = writeQueue.front();
        writeQueue.pop();
    }
    return cfio;
}

void HdcFileDescriptor::NotifyWrite()
{
    writeCond.notify_one();
}

void HdcFileDescriptor::WaitWrite()
{
    std::unique_lock<std::mutex> lock(writeMutex);
    writeCond.wait_for(lock, std::chrono::seconds(WAIT_SECONDS), [&]() {
        return !writeQueue.empty() || !workContinue;
    });
}

void HdcFileDescriptor::HandleWrite()
{
    CtxFileIO *cfio = nullptr;
    while ((cfio = PopWrite()) != nullptr) {
        CtxFileIOWrite(cfio);
        delete cfio;
    }
}

void HdcFileDescriptor::CtxFileIOWrite(CtxFileIO *cfio)
{
    std::unique_lock<std::mutex> lock(writeMutex);
    uint8_t *buf = cfio->bufIO;
    uint8_t *data = buf;
    size_t cnt = cfio->size;
    constexpr int intrmax = 1000;
    int intrcnt = 0;
    while (cnt > 0) {
        ssize_t rc = write(fdIO, data, cnt);
        if (rc < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                if (++intrcnt > intrmax) {
                    WRITE_LOG(LOG_WARN, "CtxFileIOWrite fdIO:%d interrupt errno:%d", fdIO, errno);
                    intrcnt = 0;
                }
                continue;
            } else {
                WRITE_LOG(LOG_FATAL, "CtxFileIOWrite fdIO:%d rc:%d error:%d", fdIO, rc, errno);
                break;
            }
        }
        data += rc;
        cnt -= static_cast<size_t>(rc);
    }
    delete[] buf;
}
}  // namespace Hdc
