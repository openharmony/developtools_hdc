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
#ifndef HDC_FILE_DESCRIPTOR_H
#define HDC_FILE_DESCRIPTOR_H
#include "common.h"

namespace Hdc {
class HdcFileDescriptor;
struct CtxFileIO {
    uv_fs_t fs;
    uint8_t *bufIO;
    size_t size;
    HdcFileDescriptor *thisClass;
};

class FileIoThread {
public:
    explicit FileIoThread(HdcFileDescriptor* ptr);
    ~FileIoThread();
    void Run();
private:
    HdcFileDescriptor* descriptor = nullptr;
    bool fetalFinish = false;
    uint8_t* buf = nullptr;
    int bufSize = 0;

    bool ReadData(int epollFd, void* events);
    bool Malloc();
    bool PrepareBuf();
};

class HdcFileDescriptor {
public:
    // callerContext, normalFinish, errorString
    using CmdResultCallback = std::function<bool(const void *, const bool, const string)>;
    // callerContext, readBuf, readIOByes
    using CallBackWhenRead = std::function<bool(const void *, uint8_t *, const int)>;
    HdcFileDescriptor(uv_loop_t *loopIn, int fdToRead, void *callerContextIn, CallBackWhenRead callbackReadIn,
                      CmdResultCallback callbackFinishIn, bool interactiveShell);
    virtual ~HdcFileDescriptor();
    int Write(uint8_t *data, int size);
    int WriteWithMem(uint8_t *data, int size);

    bool ReadyForRelease();
    bool StartWorkOnThread();
    void StopWorkOnThread(bool tryCloseFdIo, std::function<void()> closeFdCallback);

protected:
private:
    static void FileIOOnThread(CtxFileIO *ctxIO, int bufSize);
    int LoopReadOnThread();

    std::function<void()> callbackCloseFd;
    CmdResultCallback callbackFinish;
    CallBackWhenRead callbackRead;
    uv_loop_t *loop;
    void *callerContext;
    std::atomic<bool> workContinue;
    int fdIO;
    int refIO;
    bool isInteractive;
    std::thread ioReadThread;
    std::thread ioWriteThread;

    static void IOWriteThread(void *object);
    std::queue<CtxFileIO *> writeQueue;
    std::mutex writeMutex;
    std::condition_variable writeCond;
    void PushWrite(CtxFileIO *cfio);
    CtxFileIO *PopWrite();
    void NotifyWrite();
    void HandleWrite();
    void WaitWrite();
    void CtxFileIOWrite(CtxFileIO *cfio);

    friend class FileIoThread;
};
}  // namespace Hdc

#endif  // HDC_FILE_DESCRIPTOR_H
