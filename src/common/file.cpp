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
#include "file.h"
#include "serial_struct.h"

namespace Hdc {
HdcFile::HdcFile(HTaskInfo hTaskInfo)
    : HdcTransferBase(hTaskInfo)
{
    commandBegin = CMD_FILE_BEGIN;
    commandData = CMD_FILE_DATA;
}

HdcFile::~HdcFile()
{
    WRITE_LOG(LOG_DEBUG, "~HdcFile");
};

void HdcFile::StopTask()
{
    WRITE_LOG(LOG_DEBUG, "HdcFile StopTask");
    singalStop = true;
};

bool HdcFile::BeginTransfer(CtxFile *context, const string &command)
{
    int argc = 0;
    bool ret = false;
    char **argv = Base::SplitCommandToArgs(command.c_str(), &argc);
    if (argc < CMD_ARG1_COUNT || argv == nullptr) {
        LogMsg(MSG_FAIL, "Transfer path split failed");
        if (argv) {
            delete[](reinterpret_cast<char *>(argv));
        }
        return false;
    }
    if (!SetMasterParameters(context, command.c_str(), argc, argv)) {
        delete[](reinterpret_cast<char *>(argv));
        return false;
    }
    do {
        ++refCount;
        uv_fs_open(loopTask, &context->fsOpenReq, context->localPath.c_str(), O_RDONLY, S_IWUSR | S_IRUSR, OnFileOpen);
        context->master = true;
        ret = true;
    } while (false);
    if (!ret) {
        LogMsg(MSG_FAIL, "Transfer path failed, Master:%s Slave:%s", context->localPath.c_str(),
               context->remotePath.c_str());
    }
    delete[](reinterpret_cast<char *>(argv));
    return ret;
}

bool HdcFile::SetMasterParameters(CtxFile *context, const char *command, int argc, char **argv)
{
    int srcArgvIndex = 0;
    string errStr;
    const string cmdOptionTstmp = "-a";
    const string cmdOptionSync = "-sync";
    const string cmdOptionZip = "-z";
    const string cmdOptionModeSync = "-m";

    for (int i = 0; i < argc; i++) {
        if (argv[i] == cmdOptionZip) {
            context->transferConfig.compressType = COMPRESS_LZ4;
            ++srcArgvIndex;
        } else if (argv[i] == cmdOptionSync) {
            context->transferConfig.updateIfNew = true;
            ++srcArgvIndex;
        } else if (argv[i] == cmdOptionTstmp) {
            // The time zone difference may cause the display time on the PC and the
            // device to differ by several hours
            //
            // ls -al --full-time
            context->transferConfig.holdTimestamp = true;
            ++srcArgvIndex;
        } else if (argv[i] == CMD_OPTION_CLIENTCWD) {
            context->transferConfig.clientCwd = argv[i + 1];
            srcArgvIndex += CMD_ARG1_COUNT;  // skip 2args
        } else if (argv[i] == cmdOptionModeSync) {
            context->fileModeSync = true;
            ++srcArgvIndex;
        } else if (argv[i] == CMDSTR_REMOTE_PARAMETER) {
            ++srcArgvIndex;
        } else if (argv[i][0] == '-') {
            LogMsg(MSG_FAIL, "Unknown file option: %s", argv[i]);
            return false;
        }
    }
    if (argc == srcArgvIndex) {
        LogMsg(MSG_FAIL, "There is no local and remote path");
        return false;
    }
    context->remotePath = argv[argc - 1];
    context->localPath = argv[argc - 2];
    if (taskInfo->serverOrDaemon) {
        // master and server
        if ((srcArgvIndex + 1) == argc) {
            LogMsg(MSG_FAIL, "There is no remote path");
            return false;
        }
        ExtractRelativePath(context->transferConfig.clientCwd, context->localPath);
    } else {
        if ((srcArgvIndex + 1) == argc) {
            context->remotePath = ".";
            context->localPath = argv[argc - 1];
        }
    }

    context->localName = Base::GetFullFilePath(context->localPath);

    mode_t mode = mode_t(~S_IFMT);
    if (!Base::CheckDirectoryOrPath(context->localPath.c_str(), true, true, errStr, mode) && (mode & S_IFDIR)) {
        context->isDir = true;
        GetSubFilesRecursively(context->localPath, context->localName, &context->taskQueue);
        if (context->taskQueue.size() == 0) {
            LogMsg(MSG_FAIL, "Directory empty.");
            return false;
        }
        context->fileCnt = 0;
        context->dirSize = 0;
        context->localDirName = Base::GetPathWithoutFilename(context->localPath);

        WRITE_LOG(LOG_DEBUG, "context->localDirName = %s", context->localDirName.c_str());

        context->localName = context->taskQueue.back();
        context->localPath = context->localDirName + context->localName;

        WRITE_LOG(LOG_DEBUG, "localName = %s context->localPath = %s", context->localName.c_str(),
                  context->localPath.c_str());
        context->taskQueue.pop_back();
    }
    return true;
}

void HdcFile::CheckMaster(CtxFile *context)
{
    if (context->fileModeSync) {
        string s = SerialStruct::SerializeToString(context->fileMode);
        SendToAnother(CMD_FILE_MODE, reinterpret_cast<uint8_t *>(const_cast<char *>(s.c_str())), s.size());
    } else {
        string s = SerialStruct::SerializeToString(context->transferConfig);
        SendToAnother(CMD_FILE_CHECK, reinterpret_cast<uint8_t *>(const_cast<char *>(s.c_str())), s.size());
    }
}

void HdcFile::WhenTransferFinish(CtxFile *context)
{
    WRITE_LOG(LOG_DEBUG, "HdcTransferBase WhenTransferFinish");
    uint8_t flag = 1;
    context->fileCnt++;
    context->dirSize += context->indexIO;
    SendToAnother(CMD_FILE_FINISH, &flag, 1);
}

void HdcFile::TransferSummary(CtxFile *context)
{
    uint64_t nMSec = Base::GetRuntimeMSec() -
                     (context->fileCnt > 1 ? context->transferDirBegin : context->transferBegin);
    uint64_t fSize = context->fileCnt > 1 ? context->dirSize : context->indexIO;
    double fRate = static_cast<double>(fSize) / nMSec; // / /1000 * 1000 = 0
    if (context->indexIO >= context->fileSize) {
        WRITE_LOG(LOG_INFO, "HdcFile::TransferSummary success");
        LogMsg(MSG_OK, "FileTransfer finish, Size:%lld, File count = %d, time:%lldms rate:%.2lfkB/s",
               fSize, context->fileCnt, nMSec, fRate);
    } else {
        constexpr int bufSize = 1024;
        char buf[bufSize] = { 0 };
        uv_strerror_r(static_cast<int>(-context->lastErrno), buf, bufSize);
        LogMsg(MSG_FAIL, "Transfer Stop at:%lld/%lld(Bytes), Reason: %s", context->indexIO, context->fileSize,
               buf);
    }
}

bool HdcFile::FileModeSync(const uint16_t cmd, uint8_t *payload, const int payloadSize)
{
    if (ctxNow.master) {
        WRITE_LOG(LOG_DEBUG, "FileModeSync master ctxNow.fileModeSync = %d size = %zu", ctxNow.fileModeSync,
                  ctxNow.dirMode.size());
        if (ctxNow.dirMode.size() > 0) {
            auto mode = ctxNow.dirMode.back();
            WRITE_LOG(LOG_DEBUG, "file = %s permissions: %o u_id = %u, g_id = %u conext = %s",
                mode.fullName.c_str(), mode.perm, mode.u_id, mode.g_id, mode.context.c_str());
            string s = SerialStruct::SerializeToString(mode);
            ctxNow.dirMode.pop_back();
            SendToAnother(CMD_DIR_MODE, reinterpret_cast<uint8_t *>(const_cast<char *>(s.c_str())), s.size());
        } else {
            string s = SerialStruct::SerializeToString(ctxNow.transferConfig);
            SendToAnother(CMD_FILE_CHECK, reinterpret_cast<uint8_t *>(const_cast<char *>(s.c_str())), s.size());
        }
    } else {
        ctxNow.fileModeSync = true;
        string serialString(reinterpret_cast<char *>(payload), payloadSize);
        if (cmd == CMD_FILE_MODE) {
            SerialStruct::ParseFromString(ctxNow.fileMode, serialString);
        } else {
            FileMode dirMode;
            SerialStruct::ParseFromString(dirMode, serialString);

            WRITE_LOG(LOG_DEBUG, "file = %s permissions: %o u_id = %u, g_id = %u context = %s",
                dirMode.fullName.c_str(), dirMode.perm, dirMode.u_id, dirMode.g_id, dirMode.context.c_str());

            vector<string> dirsOfOptName;
            if (dirMode.fullName.find('/') != string::npos) {
                WRITE_LOG(LOG_DEBUG, "dir mode create parent dir from linux system");
                Base::SplitString(dirMode.fullName, "/", dirsOfOptName);
            } else if (dirMode.fullName.find('\\') != string::npos) {
                WRITE_LOG(LOG_DEBUG, "dir mode create parent dir from windows system");
                Base::SplitString(dirMode.fullName, "\\", dirsOfOptName);
            } else {
                dirsOfOptName.emplace_back(dirMode.fullName);
            }

            dirMode.fullName = "";
            for (auto s : dirsOfOptName) {
                if (dirMode.fullName.empty()) {
                    dirMode.fullName = s;
                } else {
                    dirMode.fullName = dirMode.fullName + Base::GetPathSep() + s;
                }
            }
            WRITE_LOG(LOG_DEBUG, "dir = %s permissions: %o u_id = %u, g_id = %u context = %s",
                dirMode.fullName.c_str(), dirMode.perm, dirMode.u_id, dirMode.g_id, dirMode.context.c_str());
            ctxNow.dirModeMap.insert(std::make_pair(dirMode.fullName, dirMode));
        }
        SendToAnother(CMD_FILE_MODE, nullptr, 0);
    }
    return true;
}

bool HdcFile::SlaveCheck(uint8_t *payload, const int payloadSize)
{
    bool ret = true;
    bool childRet = false;
    string errStr;
    // parse option
    string serialString(reinterpret_cast<char *>(payload), payloadSize);
    TransferConfig &stat = ctxNow.transferConfig;
    SerialStruct::ParseFromString(stat, serialString);
    ctxNow.fileSize = stat.fileSize;
    ctxNow.localPath = stat.path;
    ctxNow.master = false;
    ctxNow.fsOpenReq.data = &ctxNow;
#ifdef HDC_DEBUG
    WRITE_LOG(LOG_DEBUG, "HdcFile fileSize got %" PRIu64 "", ctxNow.fileSize);
#endif

    if (!CheckLocalPath(ctxNow.localPath, stat.optionalName, errStr)) {
        LogMsg(MSG_FAIL, "%s", errStr.c_str());
        return false;
    }

    if (!CheckFilename(ctxNow.localPath, stat.optionalName, errStr)) {
        LogMsg(MSG_FAIL, "%s", errStr.c_str());
        return false;
    }
    // check path
    childRet = SmartSlavePath(stat.clientCwd, ctxNow.localPath, stat.optionalName.c_str());
    if (childRet && ctxNow.transferConfig.updateIfNew) {  // file exist and option need update
        // if is newer
        uv_fs_t fs = {};
        uv_fs_stat(nullptr, &fs, ctxNow.localPath.c_str(), nullptr);
        uv_fs_req_cleanup(&fs);
        if ((uint64_t)fs.statbuf.st_mtim.tv_sec >= ctxNow.transferConfig.mtime) {
            LogMsg(MSG_FAIL, "Target file is the same date or newer,path: %s", ctxNow.localPath.c_str());
            return false;
        }
    }
    // begin work
    ++refCount;
    uv_fs_open(loopTask, &ctxNow.fsOpenReq, ctxNow.localPath.c_str(), UV_FS_O_TRUNC | UV_FS_O_CREAT | UV_FS_O_WRONLY,
               S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH, OnFileOpen);
    if (ctxNow.transferDirBegin == 0) {
        ctxNow.transferDirBegin = Base::GetRuntimeMSec();
    }
    ctxNow.transferBegin = Base::GetRuntimeMSec();
    return ret;
}

void HdcFile::TransferNext(CtxFile *context)
{
    WRITE_LOG(LOG_DEBUG, "HdcFile::TransferNext");

    context->localName = context->taskQueue.back();
    context->localPath = context->localDirName + context->localName;
    context->taskQueue.pop_back();
    WRITE_LOG(LOG_DEBUG, "context->localName = %s context->localPath = %s queuesize:%d",
              context->localName.c_str(), context->localPath.c_str(), ctxNow.taskQueue.size());
    do {
        ++refCount;
        uv_fs_open(loopTask, &context->fsOpenReq, context->localPath.c_str(), O_RDONLY, S_IWUSR | S_IRUSR, OnFileOpen);
    } while (false);

    return;
}

bool HdcFile::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    HdcTransferBase::CommandDispatch(command, payload, payloadSize);
    bool ret = true;
    switch (command) {
        case CMD_FILE_INIT: {  // initial
            string s = string(reinterpret_cast<char *>(payload), payloadSize);
            ret = BeginTransfer(&ctxNow, s);
            ctxNow.transferBegin = Base::GetRuntimeMSec();
            break;
        }
        case CMD_FILE_CHECK: {
            ret = SlaveCheck(payload, payloadSize);
            break;
        }
        case CMD_FILE_MODE:
        case CMD_DIR_MODE: {
            ret = FileModeSync(command, payload, payloadSize);
            break;
        }
        case CMD_FILE_FINISH: {
            if (*payload) {  // close-step3
                WRITE_LOG(LOG_DEBUG, "Dir = %d taskQueue size = %d", ctxNow.isDir, ctxNow.taskQueue.size());
                if (ctxNow.isDir && (ctxNow.taskQueue.size() > 0)) {
                    TransferNext(&ctxNow);
                } else {
                    ctxNow.ioFinish = true;
                    ctxNow.transferDirBegin = 0;
                    --(*payload);
                    SendToAnother(CMD_FILE_FINISH, payload, 1);
                }
            } else {  // close-step3
                TransferSummary(&ctxNow);
                TaskFinish();
            }
            break;
        }
        default:
            break;
    }
    return ret;
}
}  // namespace Hdc
