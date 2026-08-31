/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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
#include "file_desc_wrapper.h"
#include <atomic>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
constexpr int READ_BUF_SIZE = 65536;
constexpr int MAX_RETRY = 3;

struct FileIoContext {
    int fd = -1;
    FileReadCallback readCb = nullptr;
    FileFinishCallback finCb = nullptr;
    void *ctx = nullptr;
    std::thread readThread;
    std::thread writeThread;
    std::queue<std::vector<uint8_t>> writeQueue;
    std::mutex queueMutex;
    std::atomic<bool> running{false};
    std::atomic<bool> busy{false};
};

std::mutex g_mutex;
std::unordered_map<int, FileIoContext *> g_ctxMap;

FileIoContext *GetCtx(int fd)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_ctxMap.find(fd);
    return it != g_ctxMap.end() ? it->second : nullptr;
}

void ReadLoop(FileIoContext *ctx)
{
    std::vector<uint8_t> buf(READ_BUF_SIZE);
    while (ctx->running) {
#ifndef _WIN32
        ssize_t n = read(ctx->fd, buf.data(), buf.size());
#else
        DWORD n = 0;
        HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(ctx->fd));
        if (h == INVALID_HANDLE_VALUE) {
            break;
        }
        if (!ReadFile(h, buf.data(), static_cast<DWORD>(buf.size()), &n, nullptr)) {
            break;
        }
#endif
        if (n <= 0) {
            break;
        }
        ctx->busy = true;
        if (ctx->readCb != nullptr) {
            ctx->readCb(ctx->fd, buf.data(), static_cast<int>(n), ctx->ctx);
        }
        ctx->busy = false;
    }
    if (ctx->finCb != nullptr) {
        ctx->finCb(ctx->fd, ctx->ctx);
    }
}

void WriteLoop(FileIoContext *ctx)
{
    while (ctx->running) {
        std::vector<uint8_t> data;
        {
            std::lock_guard<std::mutex> lock(ctx->queueMutex);
            if (ctx->writeQueue.empty()) {
                continue;
            }
            data = std::move(ctx->writeQueue.front());
            ctx->writeQueue.pop();
        }
        int retry = 0;
        int offset = 0;
        while (offset < static_cast<int>(data.size()) && retry < MAX_RETRY) {
#ifndef _WIN32
            ssize_t n = write(ctx->fd, data.data() + offset, data.size() - offset);
            if (n < 0) {
                retry++;
                continue;
            }
#else
            DWORD n = 0;
            HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(ctx->fd));
            if (h == INVALID_HANDLE_VALUE) {
                break;
            }
            if (!WriteFile(h, data.data() + offset,
                           static_cast<DWORD>(data.size() - offset), &n, nullptr)) {
                retry++;
                continue;
            }
#endif
            offset += static_cast<int>(n);
        }
    }
}
}

extern "C" {
int FileDescStartRead(int fd, FileReadCallback readCb, FileFinishCallback finCb, void *ctxPtr)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_ctxMap.count(fd) > 0) {
        return -1;
    }
    auto *ctx = new FileIoContext();
    ctx->fd = fd;
    ctx->readCb = readCb;
    ctx->finCb = finCb;
    ctx->ctx = ctxPtr;
    ctx->running = true;
    g_ctxMap[fd] = ctx;
    ctx->readThread = std::thread(ReadLoop, ctx);
    ctx->writeThread = std::thread(WriteLoop, ctx);
    ctx->readThread.detach();
    ctx->writeThread.detach();
    return 0;
}

int FileDescWrite(int fd, const uint8_t *data, int len)
{
    FileIoContext *ctx = GetCtx(fd);
    if (ctx == nullptr || data == nullptr || len <= 0) {
        return -1;
    }
#ifndef _WIN32
    ssize_t n = write(fd, data, len);
    return static_cast<int>(n);
#else
    return -1;
#endif
}

int FileDescWriteQueued(int fd, const uint8_t *data, int len)
{
    FileIoContext *ctx = GetCtx(fd);
    if (ctx == nullptr || data == nullptr || len <= 0) {
        return -1;
    }
    {
        std::lock_guard<std::mutex> lock(ctx->queueMutex);
        ctx->writeQueue.push(std::vector<uint8_t>(data, data + len));
    }
    return len;
}

void FileDescStop(int fd)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_ctxMap.find(fd);
    if (it == g_ctxMap.end()) {
        return;
    }
    FileIoContext *ctx = it->second;
    ctx->running = false;
    g_ctxMap.erase(it);
    delete ctx;
}

int FileDescIsBusy(int fd)
{
    FileIoContext *ctx = GetCtx(fd);
    if (ctx == nullptr) {
        return 0;
    }
    return ctx->busy ? 1 : 0;
}
}
