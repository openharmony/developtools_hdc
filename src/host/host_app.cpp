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
#include "host_app.h"
#include "compress.h"

namespace Hdc {
HdcHostApp::HdcHostApp(HTaskInfo hTaskInfo)
    : HdcTransferBase(hTaskInfo)
{
    commandBegin = CMD_APP_BEGIN;
    commandData = CMD_APP_DATA;
    originLocalDir = "";
    isStableBuf = hTaskInfo->isStableBuf;
}

HdcHostApp::~HdcHostApp()
{
}

string HdcHostApp::Dir2Tar(const char *dir)
{
    if (Base::GetCaller() == Base::Caller::CLIENT) {
        WRITE_LOG(LOG_DEBUG, "dir:%s", dir);
    } else {
        WRITE_LOG(LOG_DEBUG, "dir:%s", Hdc::MaskString(string(dir)).c_str());
    }
    string tarpath;
    uv_fs_t req;
    int r = uv_fs_lstat(nullptr, &req, dir, nullptr);
    uv_fs_req_cleanup(&req);
    if (r == 0 && (req.statbuf.st_mode & S_IFDIR)) {  // is dir
        string sdir = dir;
        string tarname = Base::GetRandomString(EXPECTED_LEN) + ".tar";
        tarpath = Base::GetTmpDir() + tarname;
        WRITE_LOG(LOG_DEBUG, "tarpath:%s", tarpath.c_str());
        Compress c;
        c.UpdataPrefix(sdir);
        c.AddPath(sdir);
        c.SaveToFile(tarpath);
    }
    return tarpath;
}

bool HdcHostApp::BeginInstall(CtxFile *context, const char *command)
{
    int argc = 0;
    bool ret = false;
    string options;
    char **argv = Base::SplitCommandToArgs(command, &argc);
    if (argc < 1) {
        goto Finish;
    }

    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], CMD_OPTION_CLIENTCWD.c_str())) {
            if (i + 1 < argc) {
                context->transferConfig.clientCwd = argv[i + 1];
                i += 1;  // add content index
            }
        } else if (!strncmp(argv[i], "-", 1)) {
            if (options.size()) {
                options += " ";
            }
            options += argv[i];
        } else {
            string path = argv[i];
            ExtractRelativePath(context->transferConfig.clientCwd, path);
            if (MatchPackageExtendName(path, ".hap") || MatchPackageExtendName(path, ".hsp")
                || MatchPackageExtendName(path, ".app")) {
                context->taskQueue.push_back(path);
            } else {
                string tarpath = Dir2Tar(path.c_str());
                if (!tarpath.empty()) {
                    context->taskQueue.push_back(tarpath);
                    originLocalDir = path;
                }
            }
        }
    }
    if (!context->taskQueue.size()) {
        LogMsg(MSG_FAIL, "[E006001] Not any installation package was found");
        goto Finish;
    }
    // remove repeate
    sort(context->taskQueue.begin(), context->taskQueue.end());
    context->taskQueue.erase(unique(context->taskQueue.begin(), context->taskQueue.end()), context->taskQueue.end());

    context->transferConfig.options = options;
    context->transferConfig.functionName = CMDSTR_APP_INSTALL;
    ret = RunQueue(context);
Finish:
    if (argv) {
        delete[](reinterpret_cast<char *>(argv));
    }
    return ret;
}

bool HdcHostApp::BeginSideload(CtxFile *context, const char *localPath)
{
    bool ret = false;
    context->transferConfig.functionName = CMDSTR_APP_SIDELOAD;
    context->taskQueue.push_back(localPath);
    ret = RunQueue(context);
    return ret;
}

bool HdcHostApp::RunQueue(CtxFile *context)
{
    context->localPath = context->taskQueue.back();
    uv_fs_t *openReq = new uv_fs_t;
    if (openReq == nullptr) {
        LogMsg(MSG_FAIL, "HdcHostApp::RunQueue new uv_fs_t failed");
        OnFileOpenFailed(context);
        return false;
    }
    (void)memset_s(openReq, sizeof(uv_fs_t), 0, sizeof(uv_fs_t));
    openReq->data = context;
    ++refCount;
    uv_fs_open(loopTask, openReq, context->localPath.c_str(), O_RDONLY, 0, OnFileOpen);
    context->master = true;
    return true;
}

void HdcHostApp::CheckMaster(CtxFile *context)
{
    uv_fs_t fs = {};
    uv_fs_fstat(nullptr, &fs, context->openFd, nullptr);
    context->transferConfig.fileSize = fs.statbuf.st_size;
    uv_fs_req_cleanup(&fs);

    context->transferConfig.optionalName
        = Base::GetRandomString(EXPECTED_LEN);  // Prevent the name of illegal APP leads to pm unable to install
    if (context->localPath.find(".hap") != static_cast<size_t>(-1)) {
        context->transferConfig.optionalName += ".hap";
    } else if (context->localPath.find(".hsp") != static_cast<size_t>(-1)) {
        context->transferConfig.optionalName += ".hsp";
    } else if (context->localPath.find(".tar") != static_cast<size_t>(-1)) {
        context->transferConfig.optionalName += ".tar";
    } else if (context->localPath.find(".app") != static_cast<size_t>(-1)) {
        context->transferConfig.optionalName += ".app";
    } else {
        context->transferConfig.optionalName += ".bundle";
    }
    string bufString = SerialStruct::SerializeToString(context->transferConfig);
    SendToAnother(CMD_APP_CHECK, const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(bufString.c_str())),
                  bufString.size());
}

bool HdcHostApp::CheckInstallContinue(AppModType mode, const char *msg)
{
    string modeDesc;
    switch (mode) {
        case APPMOD_INSTALL:
            modeDesc = "App install";
            break;
        case APPMOD_UNINSTALL:
            modeDesc = "App uninstall";
            break;
        case APPMOD_SIDELOAD:
            modeDesc = "Side load";
            break;
        default:
            modeDesc = "Unknown";
            break;
    }
    if (ctxNow.taskQueue.size() > 0) {
        string path = ctxNow.taskQueue.back();
        ctxNow.taskQueue.pop_back();
        string::size_type pos = path.rfind(".tar");
        if (mode == APPMOD_INSTALL && pos != string::npos) {
            unlink(path.c_str());
            if (Base::GetCaller() == Base::Caller::CLIENT) {
                WRITE_LOG(LOG_DEBUG, "unlink path:%s", path.c_str());
            } else {
                WRITE_LOG(LOG_DEBUG, "unlink path:%s", Hdc::MaskString(path).c_str());
            }
        }
    }
    string path = ctxNow.localPath;
    if (!originLocalDir.empty()) {
        path = originLocalDir;
    }
    LogMsg(MSG_INFO, "%s path:%s msg:%s", modeDesc.c_str(), path.c_str(), msg + printedMsgLen);
    printedMsgLen = strlen(msg);
    if (singalStop || !ctxNow.taskQueue.size()) {
        LogMsg(MSG_OK, "AppMod finish");
        return false;
    }
    return RunQueue(&ctxNow);
}

bool HdcHostApp::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    if (!HdcTransferBase::CommandDispatch(command, payload, payloadSize)) {
        return false;
    }
    bool ret = true;
    constexpr int cmdOffset = 2;
    switch (command) {
        case CMD_APP_INIT: {
            ret = BeginInstall(&ctxNow, (const char *)payload);
            break;
        }
        case CMD_APP_FINISH: {
            AppModType mode = static_cast<AppModType>(payload[0]);
            string s(reinterpret_cast<char *>(payload + cmdOffset), payloadSize - cmdOffset);
            ret = CheckInstallContinue(mode, s.c_str());
            break;
        }
        case CMD_APP_UNINSTALL: {
            SendToAnother(CMD_APP_UNINSTALL, payload, payloadSize);
            ctxNow.taskQueue.push_back(reinterpret_cast<char *>(payload));  // just compatible
            break;
        }
        case CMD_APP_SIDELOAD: {
            BeginSideload(&ctxNow, (const char *)payload);
            break;
        }
        default:
            break;
    }
    return ret;
};
}
