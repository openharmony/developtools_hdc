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
namespace Hdc {
constexpr uint64_t HDC_TIME_CONVERT_BASE = 1000000000;


HdcTransferBase::HdcTransferBase(HTaskInfo hTaskInfo)
    : HdcTaskBase(hTaskInfo)
{
    ResetCtx(&ctxNow, true);
    commandBegin = 0;
    commandData = 0;
}

HdcTransferBase::~HdcTransferBase()
{
    WRITE_LOG(LOG_DEBUG, "~HdcTransferBase");
};

bool HdcTransferBase::ResetCtx(CtxFile *context, bool full)
{
    if (full) {
        *context = {};
        context->fsOpenReq.data = context;
        context->fsCloseReq.data = context;
        context->thisClass = this;
        context->loop = loopTask;
        context->cb = OnFileIO;
    }
    context->closeNotify = false;
    context->indexIO = 0;
    context->lastErrno = 0;
    context->ioFinish = false;
    return true;
}

int HdcTransferBase::SimpleFileIO(CtxFile *context, uint64_t index, uint8_t *sendBuf, int bytes)
{
    // The first 8 bytes file offset
    uint8_t *buf = new uint8_t[bytes + payloadPrefixReserve]();
    CtxFileIO *ioContext = new CtxFileIO();
    bool ret = false;
    while (true) {
        if (!buf || !ioContext || bytes < 0) {
            WRITE_LOG(LOG_DEBUG, "SimpleFileIO param check failed");
            break;
        }
        if (context->ioFinish) {
            WRITE_LOG(LOG_DEBUG, "SimpleFileIO to closed IOStream");
            break;
        }
        uv_fs_t *req = &ioContext->fs;
        ioContext->bufIO = buf + payloadPrefixReserve;
        ioContext->context = context;
        req->data = ioContext;
        ++refCount;
        if (context->master) {  // master just read, and slave just write.when master/read, sendBuf can be nullptr
            uv_buf_t iov = uv_buf_init(reinterpret_cast<char *>(ioContext->bufIO), bytes);
            uv_fs_read(context->loop, req, context->fsOpenReq.result, &iov, 1, index, context->cb);
        } else {
            // The US_FS_WRITE here must be brought into the actual file offset, which cannot be incorporated with local
            // accumulated index because UV_FS_WRITE will be executed multiple times and then trigger a callback.
            if (bytes > 0 && memcpy_s(ioContext->bufIO, bytes, sendBuf, bytes) != EOK) {
                WRITE_LOG(LOG_WARN, "SimpleFileIO memcpy error");
                break;
            }
            uv_buf_t iov = uv_buf_init(reinterpret_cast<char *>(ioContext->bufIO), bytes);
            uv_fs_write(context->loop, req, context->fsOpenReq.result, &iov, 1, index, context->cb);
        }
        ret = true;
        break;
    }
    if (!ret) {
        if (buf != nullptr) {
            delete[] buf;
            buf = nullptr;
        }
        if (ioContext != nullptr) {
            delete ioContext;
            ioContext = nullptr;
        }
        return -1;
    }
    return bytes;
}

void HdcTransferBase::OnFileClose(uv_fs_t *req)
{
    uv_fs_req_cleanup(req);
    CtxFile *context = (CtxFile *)req->data;
    HdcTransferBase *thisClass = (HdcTransferBase *)context->thisClass;
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
    uv_fs_futime(nullptr, &fs, context->fsOpenReq.result, aTimeSec, mTimeSec, nullptr);
    uv_fs_req_cleanup(&fs);
}

bool HdcTransferBase::SendIOPayload(CtxFile *context, uint64_t index, uint8_t *data, int dataSize)
{
    TransferPayload payloadHead;
    string head;
    int compressSize = 0;
    int sendBufSize = payloadPrefixReserve + dataSize;
    uint8_t *sendBuf = data - payloadPrefixReserve;
    bool ret = false;

    payloadHead.compressType = context->transferConfig.compressType;
    payloadHead.uncompressSize = dataSize;
    payloadHead.index = index;
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
                break;
            }
#endif
            default: {  // COMPRESS_NONE
                compressSize = dataSize;
                break;
            }
        }
    }
    payloadHead.compressSize = compressSize;
    head = SerialStruct::SerializeToString(payloadHead);
    if (head.size() + 1 > payloadPrefixReserve) {
        goto out;
    }
    if (EOK != memcpy_s(sendBuf, sendBufSize, head.c_str(), head.size() + 1)) {
        goto out;
    }
    ret = SendToAnother(commandData, sendBuf, payloadPrefixReserve + compressSize) > 0;

out:
    if (dataSize > 0 && payloadHead.compressType == COMPRESS_LZ4) {
        delete[] sendBuf;
    }
    return ret;
}

void HdcTransferBase::OnFileIO(uv_fs_t *req)
{
    CtxFileIO *contextIO = reinterpret_cast<CtxFileIO *>(req->data);
    CtxFile *context = reinterpret_cast<CtxFile *>(contextIO->context);
    HdcTransferBase *thisClass = (HdcTransferBase *)context->thisClass;
    uint8_t *bufIO = contextIO->bufIO;
    uv_fs_req_cleanup(req);
    while (true) {
        if (context->ioFinish) {
            break;
        }
        if (req->result < 0) {
            constexpr int bufSize = 1024;
            char buf[bufSize] = { 0 };
            uv_strerror_r((int)req->result, buf, bufSize);
            WRITE_LOG(LOG_DEBUG, "OnFileIO error: %s", buf);
            context->closeNotify = true;
            context->lastErrno = abs(req->result);
            context->ioFinish = true;
            break;
        }
        context->indexIO += req->result;
        if (req->fs_type == UV_FS_READ) {
#ifdef HDC_DEBUG
            WRITE_LOG(LOG_DEBUG, "read file data %" PRIu64 "/%" PRIu64 "", context->indexIO,
                      context->fileSize);
#endif // HDC_DEBUG
            if (!thisClass->SendIOPayload(context, context->indexIO - req->result, bufIO, req->result)) {
                context->ioFinish = true;
                break;
            }
            if (context->indexIO < context->fileSize) {
                thisClass->SimpleFileIO(context, context->indexIO, nullptr,
                                        Base::GetMaxBufSize() * thisClass->maxTransferBufFactor);
            } else {
                context->ioFinish = true;
            }
        } else if (req->fs_type == UV_FS_WRITE) {  // write
#ifdef HDC_DEBUG
            WRITE_LOG(LOG_DEBUG, "write file data %" PRIu64 "/%" PRIu64 "", context->indexIO,
                      context->fileSize);
#endif // HDC_DEBUG
            if (context->indexIO >= context->fileSize) {
                // The active end must first read it first, but you can't make Finish first, because Slave may not
                // end.Only slave receives complete talents Finish
                context->closeNotify = true;
                context->ioFinish = true;
                thisClass->SetFileTime(context);
            }
        } else {
            context->ioFinish = true;
        }
        break;
    }
    if (context->ioFinish) {
        // close-step1
        ++thisClass->refCount;
        if (req->fs_type == UV_FS_WRITE) {
            uv_fs_fsync(thisClass->loopTask, &context->fsCloseReq, context->fsOpenReq.result, nullptr);
        }
        WRITE_LOG(LOG_DEBUG, "channelId:%u result:%d", thisClass->taskInfo->channelId, context->fsOpenReq.result);
        uv_fs_close(thisClass->loopTask, &context->fsCloseReq, context->fsOpenReq.result, OnFileClose);
    }
    --thisClass->refCount;
    delete[] (bufIO - payloadPrefixReserve);
    delete contextIO;  // Req is part of the Contextio structure, no free release
}

void HdcTransferBase::OnFileOpen(uv_fs_t *req)
{
    CtxFile *context = (CtxFile *)req->data;
    HdcTransferBase *thisClass = (HdcTransferBase *)context->thisClass;
    uv_fs_req_cleanup(req);
    WRITE_LOG(LOG_DEBUG, "Filemod openfile:%s", context->localPath.c_str());
    --thisClass->refCount;
    if (req->result < 0) {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        uv_strerror_r((int)req->result, buf, bufSize);
        thisClass->LogMsg(MSG_FAIL, "Error opening file: %s, path:%s", buf,
                          context->localPath.c_str());
        thisClass->TaskFinish();
        return;
    }
    thisClass->ResetCtx(context);
    if (context->master) {
        // init master
        uv_fs_t fs = {};
        uv_fs_fstat(nullptr, &fs, context->fsOpenReq.result, nullptr);
        TransferConfig &st = context->transferConfig;
        st.fileSize = fs.statbuf.st_size;
        st.optionalName = context->localName;
        if (st.holdTimestamp) {
            st.atime = fs.statbuf.st_atim.tv_sec * HDC_TIME_CONVERT_BASE + fs.statbuf.st_atim.tv_nsec;
            st.mtime = fs.statbuf.st_mtim.tv_sec * HDC_TIME_CONVERT_BASE + fs.statbuf.st_mtim.tv_nsec;
        }
        st.path = context->remotePath;
        // update ctxNow=context child value
        context->fileSize = st.fileSize;

        context->fileMode.perm = fs.statbuf.st_mode;
        context->fileMode.u_id = fs.statbuf.st_uid;
        context->fileMode.g_id = fs.statbuf.st_gid;
        WRITE_LOG(LOG_DEBUG, "permissions: %o u_id = %u, g_id = %u", context->fileMode.perm,
                  context->fileMode.u_id, context->fileMode.g_id);

#if (!(defined(HOST_MINGW)||defined(HOST_MAC))) && defined(SURPPORT_SELINUX)
        char *con = nullptr;
        getfilecon(context->localPath.c_str(), &con);
        if (con != nullptr) {
            context->fileMode.context = con;
            WRITE_LOG(LOG_DEBUG, "getfilecon context = %s", con);
            freecon(con);
        }
#endif
        uv_fs_req_cleanup(&fs);
        thisClass->CheckMaster(context);
    } else {  // write
        if (context->fileModeSync) {
            FileMode &mode = context->fileMode;
            uv_fs_t fs = {};
            WRITE_LOG(LOG_DEBUG, "file mode: %o u_id = %u, g_id = %u", mode.perm, mode.u_id, mode.g_id);
            uv_fs_chmod(nullptr, &fs, context->localPath.c_str(), mode.perm, nullptr);
            uv_fs_chown(nullptr, &fs, context->localPath.c_str(), mode.u_id, mode.g_id, nullptr);
            uv_fs_req_cleanup(&fs);

#if (!(defined(HOST_MINGW)||defined(HOST_MAC))) && defined(SURPPORT_SELINUX)
            if (!mode.context.empty()) {
                WRITE_LOG(LOG_DEBUG, "setfilecon from master = %s", mode.context.c_str());
                setfilecon(context->localPath.c_str(), mode.context.c_str());
            }
#endif
        }
        thisClass->SendToAnother(thisClass->commandBegin, nullptr, 0);
    }
}

bool HdcTransferBase::MatchPackageExtendName(string fileName, string extName)
{
    bool match = false;
    int subfixIndex = fileName.rfind(extName);
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
            int subfixIndex = fileName.rfind(s);
            if ((fileName.size() - subfixIndex) != s.size())
                continue;
            string fullPath = string(path) + "/";
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

    WRITE_LOG(LOG_DEBUG, "GetSubFiles path = %s currentDirname = %s", path.c_str(), currentDirname.c_str());

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
        mode.u_id = fs.statbuf.st_uid;
        mode.g_id = fs.statbuf.st_gid;

#if (!(defined(HOST_MINGW)||defined(HOST_MAC))) && defined(SURPPORT_SELINUX)
        char *con = nullptr;
        getfilecon(path.c_str(), &con);
        if (con != nullptr) {
            mode.context = con;
            freecon(con);
        }
#endif
        ctxNow.dirMode.push_back(mode);
        WRITE_LOG(LOG_DEBUG, "dir mode: %o u_id = %u, g_id = %u, context = %s",
                  mode.perm, mode.u_id, mode.g_id, mode.context.c_str());
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
        WRITE_LOG(LOG_DEBUG, "GetSubFiles fileName = %s", fileName.c_str());

        out->push_back(currentDirname + Base::GetPathSep() + fileName);
    }
    uv_fs_req_cleanup(&req);
    return retNum;
}


bool HdcTransferBase::CheckLocalPath(string &localPath, string &optName, string &errStr)
{
    // If optName show this is directory mode, check localPath and try create each layer
    WRITE_LOG(LOG_DEBUG, "CheckDirectory localPath = %s optName = %s", localPath.c_str(), optName.c_str());
    if ((optName.find('/') == string::npos) && (optName.find('\\') == string::npos)) {
        WRITE_LOG(LOG_DEBUG, "Not directory mode optName = %s,  return", optName.c_str());
        return true;
    }
    ctxNow.isDir = true;
    uv_fs_t req;
    int r = uv_fs_lstat(nullptr, &req, localPath.c_str(), nullptr);
    mode_t mode = req.statbuf.st_mode;
    uv_fs_req_cleanup(&req);

    if (r) {
        vector<string> dirsOflocalPath;
        char sep = Base::GetPathSep();
        string split(&sep, 0, 1);
        Base::SplitString(localPath, split, dirsOflocalPath);

        WRITE_LOG(LOG_DEBUG, "localPath = %s dir layers = %zu", localPath.c_str(), dirsOflocalPath.size());
        string makedirPath;

        if (!Base::IsAbsolutePath(localPath)) {
            makedirPath = ".";
        }

        for (auto dir : dirsOflocalPath) {
            WRITE_LOG(LOG_DEBUG, "CheckLocalPath create dir = %s", dir.c_str());

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
    } else if (!(mode & S_IFDIR)) {
        WRITE_LOG(LOG_WARN, "Not a directory, path:%s", localPath.c_str());
        errStr = "Not a directory, path:";
        errStr += localPath.c_str();
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
        WRITE_LOG(LOG_DEBUG, "revise optName = %s", optName.c_str());
    }
    vector<string> dirsOfOptName;

    if (optName.find('/') != string::npos) {
        WRITE_LOG(LOG_DEBUG, "dir mode create parent dir from linux system");
        Base::SplitString(optName, "/", dirsOfOptName);
    } else if (optName.find('\\') != string::npos) {
        WRITE_LOG(LOG_DEBUG, "dir mode create parent dir from windows system");
        Base::SplitString(optName, "\\", dirsOfOptName);
    } else {
        WRITE_LOG(LOG_DEBUG, "No need create dir for file = %s", optName.c_str());
        return true;
    }

    // If filename still include dir, try create each layer
    optName = dirsOfOptName.back();
    dirsOfOptName.pop_back();

    for (auto s : dirsOfOptName) {
        // Add each layer directory to localPath
        localPath = localPath + Base::GetPathSep() + s;
        WRITE_LOG(LOG_DEBUG, "CheckFilename try create dir = %s short path = %s", localPath.c_str(), s.c_str());

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
                WRITE_LOG(LOG_DEBUG, "pos = %zu, shortPath = %s", pos, shortPath.c_str());

                // set mode
                auto it = ctxNow.dirModeMap.find(shortPath);
                if (it != ctxNow.dirModeMap.end()) {
                    auto mode = it->second;
                    WRITE_LOG(LOG_DEBUG, "file = %s permissions: %o u_id = %u, g_id = %u context = %s",
                        mode.fullName.c_str(), mode.perm, mode.u_id, mode.g_id, mode.context.c_str());

                    uv_fs_t fs = {};
                    uv_fs_chmod(nullptr, &fs, localPath.c_str(), mode.perm, nullptr);
                    uv_fs_chown(nullptr, &fs, localPath.c_str(), mode.u_id, mode.g_id, nullptr);
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

    WRITE_LOG(LOG_DEBUG, "CheckFilename finish localPath:%s optName:%s", localPath.c_str(), optName.c_str());
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
    if (r == 0 && req.statbuf.st_mode & S_IFDIR) {  // is dir
        localPath = Base::StringFormat("%s%c%s", localPath.c_str(), Base::GetPathSep(), optName);
    }
    return false;
}

bool HdcTransferBase::RecvIOPayload(CtxFile *context, uint8_t *data, int dataSize)
{
    uint8_t *clearBuf = nullptr;
    string serialStrring(reinterpret_cast<char *>(data), payloadPrefixReserve);
    TransferPayload pld;
    Base::ZeroStruct(pld);
    bool ret = false;
    SerialStruct::ParseFromString(pld, serialStrring);
    int clearSize = 0;
    if (pld.compressSize > 0) {
        switch (pld.compressType) {
#ifdef HARMONY_PROJECT
            case COMPRESS_LZ4: {
                clearBuf = new uint8_t[pld.uncompressSize]();
                if (!clearBuf) {
                    WRITE_LOG(LOG_FATAL, "alloc LZ4 buffer failed");
                    return false;
                }
                clearSize = LZ4_decompress_safe((const char *)data + payloadPrefixReserve, (char *)clearBuf,
                                                pld.compressSize, pld.uncompressSize);
                break;
            }
#endif
            default: {  // COMPRESS_NONE
                clearBuf = data + payloadPrefixReserve;
                clearSize = pld.compressSize;
                break;
            }
        }
    }
    while (true) {
        if (static_cast<uint32_t>(clearSize) != pld.uncompressSize) {
            break;
        }
        if (SimpleFileIO(context, pld.index, clearBuf, clearSize) < 0) {
            break;
        }
        ret = true;
        break;
    }
    if (pld.compressSize > 0 && pld.compressType != COMPRESS_NONE) {
        delete[] clearBuf;
    }
    return ret;
}

bool HdcTransferBase::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    bool ret = true;
    while (true) {
        if (command == commandBegin) {
            CtxFile *context = &ctxNow;
            int ioRet = SimpleFileIO(context, context->indexIO, nullptr, Base::GetMaxBufSize() * maxTransferBufFactor);
            if (ioRet < 0) {
                ret = false;
                break;
            }
            context->transferBegin = Base::GetRuntimeMSec();
        } else if (command == commandData) {
            if (static_cast<uint32_t>(payloadSize) > HDC_BUF_MAX_BYTES || payloadSize < 0) {
                ret = false;
                break;
            }
            // Note, I will trigger FileIO after multiple times.
            CtxFile *context = &ctxNow;
            if (!RecvIOPayload(context, payload, payloadSize)) {
                ret = false;
                break;
            }
        } else {
            // Other subclass commands
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
}  // namespace Hdc
