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
#include "transfer.h"
#include "serial_struct.h"
#include <sys/stat.h>
#ifdef HARMONY_PROJECT
#include <lz4.h>
#endif
#if (!(defined(HOST_MINGW)||defined(HOST_MAC))) && defined(SURPPORT_SELINUX)
#include <selinux/selinux.h>
#endif
#include <memory>
#ifdef HDC_SUPPORT_REPORT_COMMAND_EVENT
#include "command_event_report.h"
#endif

namespace Hdc {
constexpr uint64_t HDC_TIME_CONVERT_BASE = 1000000000;


HdcTransferBase::HdcTransferBase(HTaskInfo hTaskInfo)
    : HdcTaskBase(hTaskInfo)
{
    ResetCtx(&ctxNow, true);
    commandBegin = 0;
    commandData = 0;
    isStableBuf = false;
}

HdcTransferBase::~HdcTransferBase()
{
    if (ctxNow.isFdOpen) {
        WRITE_LOG(LOG_DEBUG, "~HdcTransferBase channelId:%u lastErrno:%u result:%d ioFinish:%d",
            taskInfo->channelId, ctxNow.lastErrno, ctxNow.openFd, ctxNow.ioFinish);
        
        if (ctxNow.lastErrno != 0 || (ctxNow.openFd > 0 && !ctxNow.ioFinish)) {
            CloseCtxFd(&ctxNow);
        }
    } else {
        WRITE_LOG(LOG_DEBUG, "~HdcTransferBase channelId:%u lastErrno:%u ioFinish:%d",
            taskInfo->channelId, ctxNow.lastErrno, ctxNow.ioFinish);
    }
};

bool HdcTransferBase::ResetCtx(CtxFile *context, bool full)
{
    if (full) {
        *context = {};
        context->thisClass = this;
        context->loop = loopTask;
        context->cb = OnFileIO;
    }
    context->closeNotify = false;
    context->indexIO = 0;
    context->lastErrno = 0;
    context->ioFinish = false;
    context->closeReqSubmitted = false;
    context->openFd = -1;
    return true;
}

int HdcTransferBase::SimpleFileIO(CtxFile *context, uint64_t index, uint8_t *sendBuf, int bytes)
{
    StartTraceScope("HdcTransferBase::SimpleFileIO");
    // The first 8 bytes file offset
#ifndef CONFIG_USE_JEMALLOC_DFX_INIF
    uint8_t *buf = cirbuf.Malloc();
#else
    uint8_t *buf = new uint8_t[bytes + payloadPrefixReserve]();
#endif
    if (buf == nullptr) {
        WRITE_LOG(LOG_FATAL, "SimpleFileIO buf nullptr cid:%u sid:%u", taskInfo->channelId, taskInfo->sessionId);
        return -1;
    }
    CtxFileIO *ioContext = new(std::nothrow) CtxFileIO();
    if (ioContext == nullptr) {
#ifndef CONFIG_USE_JEMALLOC_DFX_INIF
        cirbuf.Free(buf);
#else
        delete[] buf;
#endif
        WRITE_LOG(LOG_FATAL, "SimpleFileIO ioContext nullptr cid:%u sid:%u",
            taskInfo->channelId, taskInfo->sessionId);
        return -1;
    }
    bool ret = false;
    while (true) {
        size_t bufMaxSize = context->isStableBufSize ?
            static_cast<size_t>(Base::GetUsbffsBulkSizeStable() - payloadPrefixReserve) :
            static_cast<size_t>(Base::GetUsbffsBulkSize() - payloadPrefixReserve);
        if (bytes < 0 || static_cast<size_t>(bytes) > bufMaxSize) {
            WRITE_LOG(LOG_DEBUG, "SimpleFileIO param check failed cid:%u sid:%u", taskInfo->channelId,
                taskInfo->sessionId);
            break;
        }
        if (context->ioFinish) {
            WRITE_LOG(LOG_DEBUG, "SimpleFileIO to closed IOStream cid:%u sid:%u", taskInfo->channelId,
                taskInfo->sessionId);
            break;
        }
        uv_fs_t *req = &ioContext->fs;
        ioContext->bytes = static_cast<uint64_t>(bytes);
        ioContext->bufIO = buf + payloadPrefixReserve;
        ioContext->context = context;
        req->data = ioContext;
        if (context->master) {  // master just read, and slave just write.when master/read, sendBuf can be nullptr
            uv_buf_t iov = uv_buf_init(reinterpret_cast<char *>(ioContext->bufIO), bytes);
            int rc = uv_fs_read(context->loop, req, context->openFd, &iov, 1, index, context->cb);
            if (rc < 0) {
                WRITE_LOG(LOG_DEBUG, "uv_fs_read failed rc:%d", rc);
                break;
            } else {
                ++refCount;
            }
        } else {
            // The US_FS_WRITE here must be brought into the actual file offset, which cannot be incorporated with local
            // accumulated index because UV_FS_WRITE will be executed multiple times and then trigger a callback.
            if (bytes > 0 && memcpy_s(ioContext->bufIO, ioContext->bytes, sendBuf, bytes) != EOK) {
                WRITE_LOG(LOG_WARN, "SimpleFileIO memcpy error cid:%u sid:%u", taskInfo->channelId,
                    taskInfo->sessionId);
                break;
            }
            uv_buf_t iov = uv_buf_init(reinterpret_cast<char *>(ioContext->bufIO), bytes);
            int rc = uv_fs_write(context->loop, req, context->openFd, &iov, 1, index, context->cb);
            if (rc < 0) {
                WRITE_LOG(LOG_DEBUG, "uv_fs_write failed rc:%d", rc);
                break;
            } else {
                ++refCount;
            }
        }
        ret = true;
        break;
    }
    if (!ret) {
        if (ioContext != nullptr) {
            delete ioContext;
            ioContext = nullptr;
            WRITE_LOG(LOG_WARN, "SimpleFileIO ret=false, delete context, cid:%u sid:%u", taskInfo->channelId,
                taskInfo->sessionId);
        }
#ifndef CONFIG_USE_JEMALLOC_DFX_INIF
        cirbuf.Free(buf);
#else
        delete[] buf;
#endif
        return -1;
    }
    return bytes;
}

void HdcTransferBase::OnFileClose(CtxFile *context)
{
    StartTraceScope("HdcTransferBase::OnFileClose");
    context->closeReqSubmitted = false;
    HdcTransferBase *thisClass = (HdcTransferBase *)context->thisClass;
    WRITE_LOG(LOG_DEBUG, "OnFileClose channelId:%u closeNotify:%d",
              thisClass->taskInfo->channelId, context->closeNotify);
    if (context->closeNotify) {
        // close-step2
        // maybe successful finish or failed finish
        thisClass->WhenTransferFinish(context);
    }
    --thisClass->refCount;
    return;
}

void HdcTransferBase::SetFileTime(CtxFile *context)
{
    if (!context->transferConfig.holdTimestamp) {
        return;
    }
    if (!context->transferConfig.mtime) {
        return;
    }
    uv_fs_t fs;
    double aTimeSec = static_cast<long double>(context->transferConfig.atime) / HDC_TIME_CONVERT_BASE;
    double mTimeSec = static_cast<long double>(context->transferConfig.mtime) / HDC_TIME_CONVERT_BASE;
    uv_fs_futime(nullptr, &fs, context->openFd, aTimeSec, mTimeSec, nullptr);
    uv_fs_req_cleanup(&fs);
}

bool HdcTransferBase::InitTransferPayload(TransferPayload &payloadHead, uint64_t index, uint8_t compressType,
    uint32_t dataSize)
{
    payloadHead.compressType = compressType;
    payloadHead.uncompressSize = dataSize;
    payloadHead.index = index;
    return true;
}

bool HdcTransferBase::SendIOPayload(CtxFile *context, uint64_t index, uint8_t *data, int dataSize)
{
    TransferPayload payloadHead;
    string head;
    int compressSize = 0;
    int sendBufSize = payloadPrefixReserve + dataSize;
    uint8_t *sendBuf = data - payloadPrefixReserve;
    bool ret = false;

    StartTraceScope("HdcTransferBase::SendIOPayload");
    InitTransferPayload(payloadHead, index, context->transferConfig.compressType, dataSize);
    if (dataSize > 0) {
        switch (payloadHead.compressType) {
#ifdef HARMONY_PROJECT
            case COMPRESS_LZ4: {
                sendBuf = new uint8_t[sendBufSize]();
                if (!sendBuf) {
                    WRITE_LOG(LOG_FATAL, "alloc LZ4 buffer failed");
                    return false;
                }
                compressSize = LZ4_compress_default((const char *)data, (char *)sendBuf + payloadPrefixReserve,
                                                    dataSize, dataSize);
                if (compressSize <= 0) {
                    if (Base::GetCaller() == Base::Caller::CLIENT) {
                        WRITE_LOG(LOG_DEBUG, "LZ4 compress failed, path: %s compress none",
                                  context->localPath.c_str());
                    } else {
                        WRITE_LOG(LOG_DEBUG, "LZ4 compress failed, path: %s compress none",
                                  Hdc::MaskString(context->remotePath).c_str());
                    }
                    delete[] sendBuf;
                    payloadHead.compressType = COMPRESS_NONE;
                    compressSize = dataSize;
                    sendBuf = data - payloadPrefixReserve;
                }
                break;
            }
#endif
            default: {  // COMPRESS_NONE
                compressSize = dataSize;
                break;
            }
        }
    }
    payloadHead.compressSize = static_cast<uint32_t>(compressSize);
    head = SerialStruct::SerializeToString(payloadHead);
    if (head.size() + 1 > payloadPrefixReserve ||
        EOK != memcpy_s(sendBuf, sendBufSize, head.c_str(), head.size() + 1)) {
        WRITE_LOG(LOG_WARN, "fail, head size:%d, reserve:%d", head.size(), payloadPrefixReserve);
        goto out;
    }
    ret = SendToAnother(commandData, sendBuf, payloadPrefixReserve + compressSize) > 0;
out:
    if (dataSize > 0 && payloadHead.compressType == COMPRESS_LZ4) {
        delete[] sendBuf;
    }
    return ret;
}

void HdcTransferBase::ProcressFileIOFinish(uv_fs_t *req, CtxFile *context, HdcTransferBase *thisClass)
{
    // close-step1
    ++thisClass->refCount;
    if (req->fs_type == UV_FS_WRITE && context->isFdOpen) {
        uv_fs_t req = {};
        uv_fs_fsync(nullptr, &req, context->openFd, nullptr);
        uv_fs_req_cleanup(&req);
    }
    WRITE_LOG(LOG_DEBUG, "channelId:%u result:%d, closeReqSubmitted:%d, context->isFdOpen %d",
              thisClass->taskInfo->channelId, context->openFd, context->closeReqSubmitted, context->isFdOpen);
    CloseCtxFd(context);
    if (context->lastErrno == 0 && !context->closeReqSubmitted) {
        context->closeReqSubmitted = true;
        WRITE_LOG(LOG_DEBUG, "OnFileIO fs_close, channelId:%u", thisClass->taskInfo->channelId);
        OnFileClose(context);
    } else {
        thisClass->WhenTransferFinish(context);
        --thisClass->refCount;
    }
}
// return true: finished
bool HdcTransferBase::ProcressFileIOWrite(uv_fs_t *req, CtxFile *context, HdcTransferBase *thisClass)
{
    DEBUG_LOG("write file data %" PRIu64 "/%" PRIu64 "", context->indexIO, context->fileSize);
    if (context->indexIO >= context->fileSize || req->result == 0) {
        // The active end must first read it first, but you can't make Finish first, because Slave may not
        // end.Only slave receives complete talents Finish
        context->closeNotify = true;
        thisClass->SetFileTime(context);
        return true;
    }
    return false;
}
// return true: finished
bool HdcTransferBase::ProcressFileIORead(uv_fs_t *req, CtxFile *context, HdcTransferBase *thisClass)
{
    CtxFileIO *contextIO = reinterpret_cast<CtxFileIO *>(req->data);
    uint8_t *bufIO = contextIO->bufIO;
    DEBUG_LOG("read file data %" PRIu64 "/%" PRIu64 "", context->indexIO, context->fileSize);
    if (!thisClass->SendIOPayload(context, context->indexIO - req->result, bufIO, req->result)) {
        WRITE_LOG(LOG_WARN, "OnFileIO SendIOPayload fail.");
        return true;
    }
    if (req->result == 0) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_DEBUG, "path:%s fd:%d eof", context->localPath.c_str(), context->openFd);
        } else {
            WRITE_LOG(LOG_DEBUG, "path:%s fd:%d eof", Hdc::MaskString(context->remotePath).c_str(),
                      context->openFd);
        }
        return true;
    }
    if (context->indexIO < context->fileSize) {
        thisClass->SimpleFileIO(context, context->indexIO, nullptr, context->isStableBufSize ?
            (Base::GetMaxBufSizeStable() * thisClass->maxTransferBufFactor) :
            (Base::GetMaxBufSize() * thisClass->maxTransferBufFactor));
        return false;
    }
    return true;
}
// return true: finished
bool HdcTransferBase::ProcressFileIO(uv_fs_t *req, CtxFile *context, HdcTransferBase *thisClass,
    uint64_t bytes)
{
    if (context->ioFinish) {
        WRITE_LOG(LOG_DEBUG, "OnFileIO finish is true.");
        return true;
    }
    if (!ProcressFileIOIsSuccess(req, context, bytes)) {
        context->closeNotify = true;
        if (!context->master) {
            uint8_t payload = 0;
            thisClass->CommandDispatch(CMD_FILE_FINISH, &payload, sizeof(payload));
        }
        return true;
    }
    context->indexIO += static_cast<uint64_t>(req->result);
    if (req->fs_type == UV_FS_READ) {
        return ProcressFileIORead(req, context, thisClass);
    }
    if (req->fs_type == UV_FS_WRITE) {  // write
        return ProcressFileIOWrite(req, context, thisClass);
    }

    return true;
}
// check process file IO Error
bool HdcTransferBase::ProcressFileIOIsSuccess(uv_fs_t *req, CtxFile *context, uint64_t bytes)
{
    if (req->result < 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        uv_strerror_r((int)req->result, buf, bufSize);
        WRITE_LOG(LOG_DEBUG, "OnFileIO error: %s", buf);
        context->lastErrno = static_cast<uint32_t>(abs(req->result));
        return false;
    }
    if (context->master) {
        return true;
    }
    if (bytes == static_cast<uint64_t>(req->result)) {
        return true;
    }
    // On Unix platforms, uv_fs_write invoke the write function multiple times.
    // req->result only retains the errno of the first failed write attempt.
    // If the first write succeeds but a subsequent one fails,
    // req->result will reflect the total number of bytes written.
    // Therefore, if the expected number of bytes does not match req->result,
    // it indicates that a write failure has occurred.
    WRITE_LOG(LOG_WARN, "OnFileIO read bytes:%llu not equal to req result:%d", bytes, req->result);
    uv_fs_t fs = {};
    int ret = uv_fs_statfs(nullptr, &fs, context->localPath.c_str(), nullptr);
    if (ret < 0 || (!fs.ptr)) {
        constexpr int size = 1024;
        char buf[size] = { 0 };
        uv_strerror_r(ret, buf, size);
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_WARN, "CheckSpace error querying filesystem: %s, path: %s",
                      buf, context->localPath.c_str());
        } else {
            WRITE_LOG(LOG_WARN, "CheckSpace error querying filesystem: %s, path: %s",
                      buf, Hdc::MaskString(context->remotePath).c_str());
        }
        uv_fs_req_cleanup(&fs);
        context->lastErrno = static_cast<uint32_t>(abs(ret));
        return false;
    }
    uv_statfs_t* statfs = static_cast<uv_statfs_t*>(fs.ptr);
    uint64_t freeBytes = statfs->f_bsize * statfs->f_bfree;
    if (Base::GetCaller() == Base::Caller::CLIENT) {
        WRITE_LOG(LOG_DEBUG, "CheckSpace, path: %s, freeBytes: %llu",
                  context->localPath.c_str(), freeBytes);
    } else {
        WRITE_LOG(LOG_DEBUG, "CheckSpace, path: %s, freeBytes: %llu",
                  Hdc::MaskString(context->remotePath).c_str(), freeBytes);
    }
    uv_fs_req_cleanup(&fs);
    context->lastErrno = static_cast<uint32_t>((freeBytes == 0) ? ENOSPC : EIO);
    return false;
}
// return true: do io delayed
bool HdcTransferBase::IODelayed(uv_fs_t *req)
{
#ifndef HDC_HOST
    return false;
#endif
    if (req->fs_type != UV_FS_READ) {
        return false;
    }
    CtxFileIO *contextIO = reinterpret_cast<CtxFileIO *>(req->data);
    CtxFile *context = reinterpret_cast<CtxFile *>(contextIO->context);
    HdcTransferBase *thisClass = (HdcTransferBase *)context->thisClass;
    if (thisClass->taskInfo->channelTask) {
        const int maxPackages = 64;
        const int onePackageTransferTime = 2;
        const int delayPackages = maxPackages / 2;
        const int delayMs = delayPackages * onePackageTransferTime;
        HdcChannelBase *channelBase = reinterpret_cast<HdcChannelBase *>(thisClass->taskInfo->channelClass);
        if (channelBase->queuedPackages.load() >= maxPackages) {
            WRITE_LOG(LOG_DEBUG, "queued packages:%d is full", channelBase->queuedPackages.load());
            Base::DelayDo(req->loop, delayMs, 0, "ChannelFull", req,
                          [](const uint8_t flag, string &msg, const void *data) {
                              uv_fs_t *req = (uv_fs_t *)data;
                              OnFileIO(req);
                          });
            return true;
        }
        channelBase->queuedPackages.fetch_add(1, std::memory_order_relaxed);
    }
    return false;
}
void HdcTransferBase::OnFileIO(uv_fs_t *req)
{
    StartTraceScope("HdcTransferBase::OnFileIO");
    if (IODelayed(req)) {
        return;
    }
    CtxFileIO *contextIO = reinterpret_cast<CtxFileIO *>(req->data);
    CtxFile *context = reinterpret_cast<CtxFile *>(contextIO->context);
    HdcTransferBase *thisClass = (HdcTransferBase *)context->thisClass;
    CALLSTAT_GUARD(*(thisClass->loopTaskStatus), req->loop, "HdcTransferBase::OnFileIO");
    uint8_t *bufIO = contextIO->bufIO;
    uv_fs_req_cleanup(req);
    context->ioFinish = ProcressFileIO(req, context, thisClass, contextIO->bytes);
    if (context->ioFinish) {
        ProcressFileIOFinish(req, context, thisClass);
    }
#ifndef CONFIG_USE_JEMALLOC_DFX_INIF
    thisClass->cirbuf.Free(bufIO - payloadPrefixReserve);
#else
    delete [] (bufIO - payloadPrefixReserve);
#endif
    --thisClass->refCount;
    delete contextIO;  // Req is part of the Contextio structure, no free release
}

void HdcTransferBase::OnFileOpenFailed(CtxFile *context)
{
    HdcTransferBase *thisClass = (HdcTransferBase *)context->thisClass;
    if (context->isDir && context->master) {
        uint8_t payload = 1;
        thisClass->CommandDispatch(CMD_FILE_FINISH, &payload, 1);
    } else if (context->isDir && !context->master) {
        uint8_t payload = 1;
        thisClass->SendToAnother(CMD_FILE_FINISH, &payload, 1);
    } else {
        thisClass->TaskFinish();
    }
    return;
}

bool HdcTransferBase::IsValidBundlePath(const string &bundleName)
{
    string fullPath = SANDBOX_ROOT_DIR + bundleName + Base::GetPathSep();
    return Base::CheckBundleName(bundleName) && access(fullPath.c_str(), F_OK) == 0;
}

void HdcTransferBase::RemoveSandboxRootPath(std::string &srcStr, const std::string &bundleName)
{
    if (taskInfo->serverOrDaemon || !IsValidBundlePath(bundleName)) {
        return;
    }
    string fullPath = SANDBOX_ROOT_DIR + bundleName + Base::GetPathSep();
    size_t pos = 0;
    if ((pos = srcStr.find(fullPath)) != std::string::npos) {
        srcStr = srcStr.replace(pos, fullPath.length(), "");
    } else {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_DEBUG, "fullPath:%s, srcStr:%s", fullPath.c_str(), srcStr.c_str());
        } else {
            WRITE_LOG(LOG_DEBUG, "fullPath:%s, srcStr:%s", Hdc::MaskString(fullPath).c_str(),
                      Hdc::MaskString(srcStr).c_str());
        }
    }
}

void HdcTransferBase::OnFileOpen(uv_fs_t *req)
{
    StartTraceScope("HdcTransferBase::OnFileOpen");
    std::unique_ptr<uv_fs_t> uptrReq(req);
    CtxFile *context = (CtxFile *)req->data;
    HdcTransferBase *thisClass = (HdcTransferBase *)context->thisClass;
    CALLSTAT_GUARD(*(thisClass->loopTaskStatus), req->loop, "HdcTransferBase::OnFileOpen");
    uv_fs_req_cleanup(req);
    if (Base::GetCaller() == Base::Caller::CLIENT) {
        WRITE_LOG(LOG_DEBUG, "Filemod openfile:%s channelId:%u result:%d",
                  context->localPath.c_str(), thisClass->taskInfo->channelId, req->result);
    } else {
        WRITE_LOG(LOG_DEBUG, "Filemod openfile:%s channelId:%u result:%d",
                  Hdc::MaskString(context->remotePath).c_str(), thisClass->taskInfo->channelId, req->result);
    }
    --thisClass->refCount;
    if (req->result <= 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        uv_strerror_r((int)req->result, buf, bufSize);
        string localPath = context->localPath;
        thisClass->RemoveSandboxRootPath(localPath, context->bundleName);
        thisClass->LogMsg(MSG_FAIL, "Error opening file: %s, path:%s", buf,
                          localPath.c_str());
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_FATAL, "open path:%s, localPath:%s, error:%s, dir:%d, master:%d",
                      context->localPath.c_str(),
                      localPath.c_str(), buf, context->isDir, context->master);
        } else {
            WRITE_LOG(LOG_FATAL, "open path:%s, localPath:%s, error:%s, dir:%d, master:%d",
                      Hdc::MaskString(context->remotePath).c_str(),
                      Hdc::MaskString(localPath).c_str(), buf, context->isDir, context->master);
        }
        OnFileOpenFailed(context);
        return;
    }
    thisClass->ResetCtx(context);
    context->isFdOpen = true;
    context->openFd = req->result;
    if (context->master) { // master just read, and slave just write.
        // init master
        uv_fs_t fs = {};
        uv_fs_fstat(nullptr, &fs, context->openFd, nullptr);
        WRITE_LOG(LOG_DEBUG, "uv_fs_fstat result:%d fileSize:%llu",
            context->openFd, fs.statbuf.st_size);
        TransferConfig &st = context->transferConfig;
        st.fileSize = fs.statbuf.st_size;
        st.optionalName = context->localName;
        if (st.holdTimestamp) {
            st.atime = static_cast<uint64_t>(fs.statbuf.st_atim.tv_sec * HDC_TIME_CONVERT_BASE
                                             + fs.statbuf.st_atim.tv_nsec);
            st.mtime = static_cast<uint64_t>(fs.statbuf.st_mtim.tv_sec * HDC_TIME_CONVERT_BASE
                                             + fs.statbuf.st_mtim.tv_nsec);
        }
        st.path = context->remotePath;
        // update ctxNow=context child value
        context->fileSize = st.fileSize;
        context->fileMode.perm = fs.statbuf.st_mode;
        context->fileMode.uId = fs.statbuf.st_uid;
        context->fileMode.gId = fs.statbuf.st_gid;
#if (!(defined(HOST_MINGW)||defined(HOST_MAC))) && defined(SURPPORT_SELINUX)
        char *con = nullptr;
        getfilecon(context->localPath.c_str(), &con);
        if (con != nullptr) {
            context->fileMode.context = con;
            freecon(con);
        }
#endif
        uv_fs_req_cleanup(&fs);
        thisClass->CheckMaster(context);
    } else {  // write
        if (context->fileModeSync) {
            FileMode &mode = context->fileMode;
            uv_fs_t fs = {};
            uv_fs_chmod(nullptr, &fs, context->localPath.c_str(), mode.perm, nullptr);
            uv_fs_chown(nullptr, &fs, context->localPath.c_str(), mode.uId, mode.gId, nullptr);
            uv_fs_req_cleanup(&fs);

#if (!(defined(HOST_MINGW)||defined(HOST_MAC))) && defined(SURPPORT_SELINUX)
            if (!mode.context.empty()) {
                WRITE_LOG(LOG_DEBUG, "setfilecon from master = %s", mode.context.c_str());
                setfilecon(context->localPath.c_str(), mode.context.c_str());
            }
#endif
        }
        union FeatureFlagsUnion f{};
        if (!thisClass->AddFeatures(f)) {
            WRITE_LOG(LOG_FATAL, "AddFeatureFlag failed");
            thisClass->SendToAnother(thisClass->commandBegin, nullptr, 0);
        } else {
            thisClass->SendToAnother(thisClass->commandBegin, f.raw, sizeof(f));
        }
    }
}

bool HdcTransferBase::MatchPackageExtendName(string fileName, string extName)
{
    bool match = false;
    size_t subfixIndex = fileName.rfind(extName);
    if (subfixIndex == string::npos) {
        return false;
    }
    if ((fileName.size() - subfixIndex) != extName.size()) {
        return false;
    }
    match = true;
    return match;
}

// filter can be empty
int HdcTransferBase::GetSubFiles(const char *path, string filter, vector<string> *out)
{
    int retNum = 0;
    uv_fs_t req = {};
    uv_dirent_t dent;
    vector<string> filterStrings;
    if (!strlen(path)) {
        return retNum;
    }
    if (filter.size()) {
        Base::SplitString(filter, ";", filterStrings);
    }

    if (uv_fs_scandir(nullptr, &req, path, 0, nullptr) < 0) {
        uv_fs_req_cleanup(&req);
        return retNum;
    }
    while (uv_fs_scandir_next(&req, &dent) != UV_EOF) {
        // Skip. File
        if (strcmp(dent.name, ".") == 0 || strcmp(dent.name, "..") == 0) {
            continue;
        }
        if (!(static_cast<uint32_t>(dent.type) & UV_DIRENT_FILE)) {
            continue;
        }
        string fileName = dent.name;
        for (auto &&s : filterStrings) {
            size_t subfixIndex = fileName.rfind(s);
            if ((fileName.size() - subfixIndex) != s.size())
                continue;
            string fullPath = string(path) + Base::GetPathSep();
            fullPath += fileName;
            out->push_back(fullPath);
            ++retNum;
        }
    }
    uv_fs_req_cleanup(&req);
    return retNum;
}


int HdcTransferBase::GetSubFilesRecursively(string path, string currentDirname, vector<string> *out)
{
    int retNum = 0;
    uv_fs_t req = {};
    uv_dirent_t dent;

    if (Base::GetCaller() == Base::Caller::CLIENT) {
        WRITE_LOG(LOG_DEBUG, "GetSubFiles path = %s currentDirname = %s", path.c_str(), currentDirname.c_str());
    } else {
        WRITE_LOG(LOG_DEBUG, "GetSubFiles path = %s currentDirname = %s",
                  Hdc::MaskString(path).c_str(), Hdc::MaskString(currentDirname).c_str());
    }

    if (!path.size()) {
        return retNum;
    }

    if (uv_fs_scandir(nullptr, &req, path.c_str(), 0, nullptr) < 0) {
        uv_fs_req_cleanup(&req);
        return retNum;
    }

    uv_fs_t fs = {};
    int ret = uv_fs_stat(nullptr, &fs, path.c_str(), nullptr);
    if (ret == 0) {
        FileMode mode;
        mode.fullName = currentDirname;
        mode.perm = fs.statbuf.st_mode;
        mode.uId = fs.statbuf.st_uid;
        mode.gId = fs.statbuf.st_gid;

#if (!(defined(HOST_MINGW)||defined(HOST_MAC))) && defined(SURPPORT_SELINUX)
        char *con = nullptr;
        getfilecon(path.c_str(), &con);
        if (con != nullptr) {
            mode.context = con;
            freecon(con);
        }
#endif
        ctxNow.dirMode.push_back(mode);
    }
    while (uv_fs_scandir_next(&req, &dent) != UV_EOF) {
        // Skip. File
        if (strcmp(dent.name, ".") == 0 || strcmp(dent.name, "..") == 0) {
            continue;
        }
        if (!(static_cast<uint32_t>(dent.type) & UV_DIRENT_FILE)) {
            WRITE_LOG(LOG_DEBUG, "subdir dent.name fileName = %s", dent.name);
            GetSubFilesRecursively(path + Base::GetPathSep() + dent.name,
                currentDirname + Base::GetPathSep() + dent.name, out);
            continue;
        }
        string fileName = dent.name;
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_DEBUG, "GetSubFiles fileName = %s", fileName.c_str());
        } else {
            WRITE_LOG(LOG_DEBUG, "GetSubFiles fileName = %s", Hdc::MaskString(fileName).c_str());
        }

        out->push_back(currentDirname + Base::GetPathSep() + fileName);
    }
    uv_fs_req_cleanup(&req);
    return retNum;
}


bool HdcTransferBase::CheckLocalPath(string &localPath, string &optName, string &errStr)
{
    // If optName show this is directory mode, check localPath and try create each layer
    if (Base::GetCaller() == Base::Caller::CLIENT) {
        WRITE_LOG(LOG_DEBUG, "CheckDirectory localPath = %s optName = %s",
                  localPath.c_str(), optName.c_str());
    } else {
        WRITE_LOG(LOG_DEBUG, "CheckDirectory localPath = %s optName = %s",
                  Hdc::MaskString(localPath).c_str(), Hdc::MaskString(optName).c_str());
    }
    if ((optName.find('/') == string::npos) && (optName.find('\\') == string::npos)) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_DEBUG, "Not directory mode optName = %s, return", optName.c_str());
        } else {
            WRITE_LOG(LOG_DEBUG, "Not directory mode optName = %s, return",
                      Hdc::MaskString(optName).c_str());
        }
        return true;
    } // when transfer single file, slaver will not create each layer directory.
    ctxNow.isDir = true;
    uv_fs_t req;
    int r = uv_fs_lstat(nullptr, &req, localPath.c_str(), nullptr);
    mode_t mode = req.statbuf.st_mode;
    uv_fs_req_cleanup(&req);
    if (r) {
        vector<string> dirsOflocalPath;
        string split(1, Base::GetPathSep());
        Base::SplitString(localPath, split, dirsOflocalPath);
        if (dirsOflocalPath.size() > 0 && !ctxNow.isDir && localPath.back() != Base::GetPathSep()) {
            dirsOflocalPath.pop_back();
        }
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_DEBUG, "localPath = %s dir layers = %zu", localPath.c_str(), dirsOflocalPath.size());
        } else {
            WRITE_LOG(LOG_DEBUG, "localPath = %s dir layers = %zu",
                      Hdc::MaskString(localPath).c_str(), dirsOflocalPath.size());
        }
        string makedirPath;
        if (!Base::IsAbsolutePath(localPath)) {
            makedirPath = ".";
        }
        for (auto dir : dirsOflocalPath) {
            if (Base::GetCaller() == Base::Caller::CLIENT) {
                WRITE_LOG(LOG_DEBUG, "CheckLocalPath create dir = %s", dir.c_str());
            } else {
                WRITE_LOG(LOG_DEBUG, "CheckLocalPath create dir = %s", Hdc::MaskString(dir).c_str());
            }

            if (dir == ".") {
                continue;
            } else {
#ifdef _WIN32
                if (dir.find(":") == 1) {
                    makedirPath = dir;
                    continue;
                }
#endif
                makedirPath = makedirPath + Base::GetPathSep() + dir;
                if (!Base::TryCreateDirectory(makedirPath, errStr)) {
                    return false;
                }
            }
        }
        // set flag to remove first layer directory of filename from master
        ctxNow.targetDirNotExist = true;
    } else if (ctxNow.isDir && !(mode & S_IFDIR)) {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_WARN, "Not a directory, path:%s", localPath.c_str());
        } else {
            WRITE_LOG(LOG_WARN, "Not a directory, path:%s", Hdc::MaskString(localPath).c_str());
        }
        errStr = "Not a directory, path:" + localPath;
        return false;
    }
    return true;
}

bool HdcTransferBase::CheckFilename(string &localPath, string &optName, string &errStr)
{
    string localPathBackup = localPath;
    if (ctxNow.targetDirNotExist) {
        // If target directory not exist, the first layer directory from master should remove
        if (optName.find('/') != string::npos) {
            optName = optName.substr(optName.find('/') + 1);
        } else if (optName.find('\\') != string::npos) {
            optName = optName.substr(optName.find('\\') + 1);
        }
    }
    vector<string> dirsOfOptName;

    if (optName.find('/') != string::npos) {
        Base::SplitString(optName, "/", dirsOfOptName);
    } else if (optName.find('\\') != string::npos) {
        Base::SplitString(optName, "\\", dirsOfOptName);
    } else {
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_DEBUG, "No need create dir for file = %s", optName.c_str());
        } else {
            WRITE_LOG(LOG_DEBUG, "No need create dir for file = %s", Hdc::MaskString(optName).c_str());
        }
        return true;
    }

    // If filename still include dir, try create each layer
    optName = dirsOfOptName.back();
    dirsOfOptName.pop_back();

    for (auto s : dirsOfOptName) {
        // Add each layer directory to localPath
        localPath = localPath + Base::GetPathSep() + s;
        if (!Base::TryCreateDirectory(localPath, errStr)) {
            return false;
        }
        if (ctxNow.fileModeSync) {
            string resolvedPath = Base::CanonicalizeSpecPath(localPath);
            auto pos = resolvedPath.find(localPathBackup);
            if (pos == 0) {
                string shortPath = resolvedPath.substr(localPathBackup.size());
                if (shortPath.at(0) == Base::GetPathSep()) {
                    shortPath = shortPath.substr(1);
                }
                if (Base::GetCaller() == Base::Caller::CLIENT) {
                    WRITE_LOG(LOG_DEBUG, "pos = %zu, shortPath = %s", pos, shortPath.c_str());
                } else {
                    WRITE_LOG(LOG_DEBUG, "pos = %zu, shortPath = %s", pos, Hdc::MaskString(shortPath).c_str());
                }

                // set mode
                auto it = ctxNow.dirModeMap.find(shortPath);
                if (it != ctxNow.dirModeMap.end()) {
                    auto mode = it->second;
                    uv_fs_t fs = {};
                    uv_fs_chmod(nullptr, &fs, localPath.c_str(), mode.perm, nullptr);
                    uv_fs_chown(nullptr, &fs, localPath.c_str(), mode.uId, mode.gId, nullptr);
                    uv_fs_req_cleanup(&fs);
#if (!(defined(HOST_MINGW) || defined(HOST_MAC))) && defined(SURPPORT_SELINUX)
                    if (!mode.context.empty()) {
                        WRITE_LOG(LOG_DEBUG, "setfilecon from master = %s", mode.context.c_str());
                        setfilecon(localPath.c_str(), mode.context.c_str());
                    }
#endif
                }
            }
        }
    }

    if (Base::GetCaller() == Base::Caller::CLIENT) {
        WRITE_LOG(LOG_DEBUG, "CheckFilename finish localPath:%s optName:%s",
                  localPath.c_str(), optName.c_str());
    } else {
        WRITE_LOG(LOG_DEBUG, "CheckFilename finish localPath:%s optName:%s",
                  Hdc::MaskString(localPath).c_str(), optName.c_str());
    }
    return true;
}

// https://en.cppreference.com/w/cpp/filesystem/is_directory
// return true if file exist， false if file not exist
bool HdcTransferBase::SmartSlavePath(string &cwd, string &localPath, const char *optName)
{
    string errStr;
    if (taskInfo->serverOrDaemon) {
        // slave and server
        ExtractRelativePath(cwd, localPath);
    }
    mode_t mode = mode_t(~S_IFMT);
    if (Base::CheckDirectoryOrPath(localPath.c_str(), true, false, errStr, mode)) {
        WRITE_LOG(LOG_DEBUG, "%s", errStr.c_str());
        return true;
    }

    uv_fs_t req;
    int r = uv_fs_lstat(nullptr, &req, localPath.c_str(), nullptr);
    uv_fs_req_cleanup(&req);
    if (r == 0 && (req.statbuf.st_mode & S_IFDIR)) {  // is dir
        localPath = localPath + Base::GetPathSep() + optName;
    }
    if (r != 0 && (localPath.back() == Base::GetPathSep())) { // not exist and is dir
        localPath = localPath + optName;
    }
    return false;
}

bool HdcTransferBase::RecvIOPayload(CtxFile *context, uint8_t *data, int dataSize)
{
    if (dataSize < static_cast<int>(payloadPrefixReserve)) {
        WRITE_LOG(LOG_WARN, "unable to parse TransferPayload: invalid dataSize %d", dataSize);
        return false;
    }
    uint8_t *clearBuf = nullptr;
    string serialString(reinterpret_cast<char *>(data), payloadPrefixReserve);
    TransferPayload pld;
    (void)memset_s(&pld, sizeof(TransferPayload), 0, sizeof(TransferPayload));
    bool ret = false;
    SerialStruct::ParseFromString(pld, serialString);
    int clearSize = 0;
    StartTraceScope("HdcTransferBase::RecvIOPayload");
    if ((pld.compressSize > (static_cast<uint32_t>(dataSize) - payloadPrefixReserve)) ||
        pld.uncompressSize > MAX_SIZE_IOBUF) {
        WRITE_LOG(LOG_FATAL, "compress size is greater than the dataSize. pld.compressSize = %d", pld.compressSize);
        return false;
    }
    bool isClearBufAllocated = false;
    if (pld.compressSize > 0) {
        switch (pld.compressType) {
#ifdef HARMONY_PROJECT
            case COMPRESS_LZ4: {
                clearBuf = new uint8_t[pld.uncompressSize]();
                if (!clearBuf) {
                    WRITE_LOG(LOG_FATAL, "alloc LZ4 buffer failed");
                    return false;
                }
                isClearBufAllocated = true;
                clearSize = LZ4_decompress_safe((const char *)data + payloadPrefixReserve, (char *)clearBuf,
                                                pld.compressSize, pld.uncompressSize);
                break;
            }
#endif
            default: {  // COMPRESS_NONE
                clearBuf = data + payloadPrefixReserve;
                clearSize = static_cast<int>(pld.compressSize);
                break;
            }
        }
    }
    while (true) {
        if (clearSize < 0) {
            WRITE_LOG(LOG_WARN, "invalid data size for fileIO: %d", clearSize);
            break;
        }
        if (static_cast<uint32_t>(clearSize) != pld.uncompressSize) {
            WRITE_LOG(LOG_WARN, "invalid data size for fileIO: %d", clearSize);
            break;
        }
        if (pld.compressType == COMPRESS_NONE && dataSize - payloadPrefixReserve < clearSize) {
            WRITE_LOG(LOG_WARN, "not enough data size for fileIO dataSize: %d, clearSize: %d",
                      dataSize - payloadPrefixReserve, clearSize);
            break;
        }
        if (SimpleFileIO(context, pld.index, clearBuf, clearSize) < 0) {
            WRITE_LOG(LOG_WARN, "RecvIOPayload SimpleFileIO fail.");
            break;
        }
        ret = true;
        break;
    }
    if (isClearBufAllocated && (clearBuf != nullptr)) {
        delete[] clearBuf;
    }
    return ret;
}

bool ReportFileCommandEvent(const std::string &localPath, bool master, bool serverOrDaemon)
{
#ifdef HDC_SUPPORT_REPORT_COMMAND_EVENT
    if (!DelayedSingleton<CommandEventReport>::GetInstance()->ReportFileCommandEvent(
        localPath, master, serverOrDaemon)) {
        WRITE_LOG(LOG_FATAL,
            "[E00C002]Execution intercepted due to inaccessibility of reporting command event.");
        return false;
    }
#endif
    return true;
}

bool HdcTransferBase::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    StartTraceScope("HdcTransferBase::CommandDispatch");
    bool ret = true;
    while (true) {
        if (command == commandBegin) {
            CtxFile *context = &ctxNow;
            if (!CheckFeatures(context, payload, payloadSize)) {
                WRITE_LOG(LOG_FATAL, "CommandDispatch CheckFeatures command:%u", command);
                ret = false;
                break;
            }
            if (!CheckSandboxOptionCompatibility(cmdBundleName, context)) {
                ret = false;
                break;
            }
            int ioRet = SimpleFileIO(context, context->indexIO, nullptr, (context->isStableBufSize) ?
                Base::GetMaxBufSizeStable() * maxTransferBufFactor :
                Base::GetMaxBufSize() * maxTransferBufFactor);
            if (ioRet < 0) {
                WRITE_LOG(LOG_FATAL, "CommandDispatch SimpleFileIO ioRet:%d", ioRet);
                ret = false;
                break;
            }
            context->transferBegin = Base::GetRuntimeMSec();
            if (!ReportFileCommandEvent(context->localPath, context->master, taskInfo->serverOrDaemon)) {
                ret = false;
                break;
            }
        } else if (command == commandData) {
            if (static_cast<uint32_t>(payloadSize) > HDC_BUF_MAX_BYTES || payloadSize < 0) {
                WRITE_LOG(LOG_FATAL, "CommandDispatch payloadSize:%d", payloadSize);
                ret = false;
                break;
            }
            CtxFile *context = &ctxNow;
            if (!RecvIOPayload(context, payload, payloadSize)) {
                WRITE_LOG(LOG_DEBUG, "RecvIOPayload return false. channelId:%u lastErrno:%u result:%d isFdOpen %d",
                    taskInfo->channelId, ctxNow.lastErrno, ctxNow.openFd, ctxNow.isFdOpen);
                CloseCtxFd(&ctxNow);
                ret = false;
                break;
            }
        } else {
        }
        break;
    }
    return ret;
}

void HdcTransferBase::ExtractRelativePath(string &cwd, string &path)
{
    bool absPath = Base::IsAbsolutePath(path);
    if (!absPath) {
        path = cwd + path;
    }
}

bool HdcTransferBase::AddFeatures(FeatureFlagsUnion &feature)
{
    feature.bits.hugeBuf = !isStableBuf;
    feature.bits.reserveBits1 = 1;
    return true;
}

bool HdcTransferBase::CheckFeatures(CtxFile *context, uint8_t *payload, const int payloadSize)
{
    if (payloadSize == FEATURE_FLAG_MAX_SIZE) {
        union FeatureFlagsUnion feature{};
        if (memcpy_s(&feature, sizeof(feature), payload, payloadSize) != EOK) {
            WRITE_LOG(LOG_FATAL, "CheckFeatures memcpy_s failed");
            return false;
        }
        WRITE_LOG(LOG_DEBUG, "isStableBuf:%d, hugeBuf:%d", isStableBuf, feature.bits.hugeBuf);
        context->isStableBufSize = isStableBuf ? true : (!feature.bits.hugeBuf);
        context->isOtherSideSandboxSupported = feature.bits.reserveBits1 > 0;
        return true;
    } else if (payloadSize == 0) {
        WRITE_LOG(LOG_DEBUG, "FileBegin CheckFeatures payloadSize:%d, use default feature.", payloadSize);
        context->isStableBufSize = true;
        context->isOtherSideSandboxSupported = false;
        return true;
    } else {
        WRITE_LOG(LOG_FATAL, "CheckFeatures payloadSize:%d", payloadSize);
        return false;
    }
}

bool HdcTransferBase::CheckSandboxOptionCompatibility(const string &option, CtxFile *context)
{
    if (option == cmdBundleName && !context->isOtherSideSandboxSupported && context->sandboxMode) {
        const char* name = taskInfo->serverOrDaemon ? "Device ROM" : "SDK";
        WRITE_LOG(LOG_DEBUG, "%s doesn't support %s option.", name, option.c_str());
        LogMsg(MSG_FAIL, "[E005004] %s doesn't support %s option.", name, option.c_str());
        OnFileOpenFailed(context);
        TaskFinish();
        return false;
    }
    return true;
}
}  // namespace Hdc
