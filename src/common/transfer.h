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
#ifndef HDC_TRANSFER_H
#define HDC_TRANSFER_H
#include "common.h"

namespace Hdc {
class HdcTransferBase : public HdcTaskBase {
public:
    enum CompressType { COMPRESS_NONE, COMPRESS_LZ4, COMPRESS_LZ77, COMPRESS_LZMA, COMPRESS_BROTLI };
    // used for child class
    struct TransferConfig {
        uint64_t fileSize;
        uint64_t atime;  // ns
        uint64_t mtime;  // ns
        string options;
        string path;
        string optionalName;
        bool updateIfNew;
        uint8_t compressType;
        bool holdTimestamp;
        string functionName;
        string clientCwd;
        string reserve1;
        string reserve2;
    };
    struct FileMode {
        uint64_t perm;
        uint64_t uId;
        uint64_t gId;
        string context;
        string fullName;
    };
    // used for HdcTransferBase. just base class use, not public
    struct TransferPayload {
        uint64_t index;
        uint8_t compressType;
        uint32_t compressSize;
        uint32_t uncompressSize;
    };
    union FeatureFlagsUnion {
        struct {
            uint8_t hugeBuf : 1; // bit 1: enable huge buffer 512K
            uint8_t compressLz4 : 1; // bit 2: enable compress default is lz4
            uint8_t reserveBits1 : 6; // bit 3-8: reserved
            uint8_t reserveBits2 : 8; // bit 9-16: reserved
            uint16_t reserveBits3 : 16; // bit 17-32: reserved
            uint32_t reserveBits4 : 32; // bit 33-64: reserved
        } bits;
        uint8_t raw[FEATURE_FLAG_MAX_SIZE];
    };
    // FEATURE_FLAG_MAX_SIZE * 8 bits = 8 * 8 bits = 64 bits
    // is used for HdcTransferBase. just base class use, not public
    HdcTransferBase(HTaskInfo hTaskInfo);
    virtual ~HdcTransferBase();
    virtual void StopTask()
    {
    }
    bool CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize);
    const string cmdOptionTstmp = "-a";
    const string cmdOptionSync = "-sync";
    const string cmdOptionZip = "-z";
    const string cmdOptionModeSync = "-m";
    const string cmdBundleName = "-b";

protected:
    // Static file context
    struct CtxFile {  // The structure cannot be initialized by MEMSET, will rename to CtxTransfer
        uint64_t fileSize;
        uint64_t dirSize;
        uint64_t indexIO; // Id or written IO bytes
        uint32_t fileCnt; // add for directory mode
        bool isDir;       // add for directory mode
        bool targetDirNotExist;
        uint64_t transferBegin;
        uint64_t transferDirBegin;
        string localName;
        string localPath;
        string remotePath;
        string localDirName;
        bool fileModeSync;
        bool master;  // Document transmission initiative
        bool closeNotify;
        bool ioFinish;
        bool closeReqSubmitted;
        bool isStableBufSize; // USB IO buffer size set stable value, false: 512K, true: 61K
        bool isFdOpen;
        void *thisClass;
        uint32_t lastErrno;
        uv_loop_t *loop;
        uv_fs_t fsOpenReq;
        uv_fs_t fsCloseReq;
        uv_fs_t fsSyncReq;
        uv_fs_cb cb;
        vector<string> taskQueue;  // save file list if directory send mode
        TransferConfig transferConfig;  // Used for network IO configuration initialization
        FileMode fileMode;
        vector<FileMode> dirMode; // save dir mode on master
        map<string, FileMode> dirModeMap; // save dir mode on slave
        ssize_t openFd;
        bool sandboxMode;
        string bundleName;
        string inputLocalPath;
        bool isOtherSideSandboxSupported;
        bool isSend = false;
    };
    // Just app-mode use
    enum AppModType {
        APPMOD_NONE,
        APPMOD_INSTALL,
        APPMOD_UNINSTALL,
        APPMOD_SIDELOAD,
    };

    static void OnFileOpen(uv_fs_t *req);
    static void OnFileOpenFailed(CtxFile *context);
    static void OnFileClose(CtxFile *context);
    bool CheckSandboxOptionCompatibility(const string &option, CtxFile *context);
    int GetSubFiles(const char *path, string filter, vector<string> *out);
    int GetSubFilesRecursively(string path, string currentDirname, vector<string> *out);
    virtual void CheckMaster(CtxFile* /* context */)
    {
    }
    virtual void WhenTransferFinish(CtxFile* /* context */)
    {
    }
    bool MatchPackageExtendName(string fileName, string extName);
    bool ResetCtx(CtxFile *context, bool full = false);
    bool SmartSlavePath(string &cwd, string &localPath, const char *optName);
    bool CheckLocalPath(string &localPath, string &optName, string &errStr);
    bool CheckFilename(string &localPath, string &optName, string &errStr);
    void SetFileTime(CtxFile *context);
    void ExtractRelativePath(string &cwd, string &path);
    bool AddFeatures(FeatureFlagsUnion &feature);
    bool CheckFeatures(CtxFile *context, uint8_t *payload, const int payloadSize);
    static void CloseCtxFd(CtxFile *context)
    {
        if (context == nullptr || !context->isFdOpen) {
            return;
        }
        if (Base::GetCaller() == Base::Caller::CLIENT) {
            WRITE_LOG(LOG_DEBUG, "CloseCtxFd, localPath:%s result:%d, closeReqSubmitted:%d",
                      context->localPath.c_str(), context->openFd, context->closeReqSubmitted);
        } else {
            WRITE_LOG(LOG_DEBUG, "CloseCtxFd, localPath:%s result:%d, closeReqSubmitted:%d",
                      Hdc::MaskString(context->localPath).c_str(), context->openFd, context->closeReqSubmitted);
        }
        uv_fs_t fs;
        uv_fs_close(nullptr, &fs, context->openFd, nullptr);
        uv_fs_req_cleanup(&fs);
        // solve the fd leak caused by early exit due to illegal operation on a directory.
        context->isFdOpen = false;
    }
    void RemoveSandboxRootPath(std::string &srcStr, const std::string &bundleName);
    bool IsValidBundlePath(const string &bundleName);

    CtxFile ctxNow;
    uint16_t commandBegin;
    uint16_t commandData;
    bool isStableBuf;
    const string CMD_OPTION_CLIENTCWD = "-cwd";
    const string SANDBOX_ROOT_DIR = "/mnt/debug/100/debug_hap/";
#ifndef CONFIG_USE_JEMALLOC_DFX_INIF
    CircleBuffer cirbuf;
#endif

private:
    // dynamic IO context
    struct CtxFileIO {
        uv_fs_t fs;
        uint8_t *bufIO;
        CtxFile *context;
        uint64_t bytes;
    };
    static const uint8_t payloadPrefixReserve = 64;
    static void OnFileIO(uv_fs_t *req);
    static bool IODelayed(uv_fs_t *req);
    static bool ProcressFileIO(uv_fs_t *req, CtxFile *context, HdcTransferBase *thisClass, uint64_t bytes);
    static bool ProcressFileIORead(uv_fs_t *req, CtxFile *context, HdcTransferBase *thisClass);
    static bool ProcressFileIOWrite(uv_fs_t *req, CtxFile *context, HdcTransferBase *thisClass);
    static void ProcressFileIOFinish(uv_fs_t *req, CtxFile *context, HdcTransferBase *thisClass);
    static bool ProcressFileIOIsSuccess(uv_fs_t *req, CtxFile *context, uint64_t bytes);
    int SimpleFileIO(CtxFile *context, uint64_t index, uint8_t *sendBuf, int bytes);
    bool SendIOPayload(CtxFile *context, uint64_t index, uint8_t *data, int dataSize);
    bool RecvIOPayload(CtxFile *context, uint8_t *data, int dataSize);
    bool InitTransferPayload(TransferPayload &payloadHead, uint64_t index, uint8_t compressType,
        uint32_t dataSize);
    double maxTransferBufFactor = 0.8;  // Make the data sent by each IO in one hdc packet
};
}  // namespace Hdc

#endif
