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

#include "host_shell_option.h"

namespace Hdc {
namespace HostShellOption {
bool FormatParametersToTlv(const string &parameters, const int startPos, string &formatOutput, string &errMsg)
{
    if (parameters.empty() || startPos >= parameters.size()) {
        errMsg = "[E003007] Internal error: Invalid option parameters";
        return false;
    }
    int argc = 0;
    char **argv = Base::SplitCommandToArgs(parameters.c_str() + startPos, &argc);
    if (argv == nullptr || argc <= 0) {
        WRITE_LOG(LOG_FATAL, "Failed to split command to args");
        errMsg = "[E003007] Internal error: Invalid option parameters";
        return false;
    }
    bool ret = false;
    TlvBuf tb(Base::REGISTERD_TAG_SET);
    if (!ParameterToTlv(argv, argc, tb, errMsg)) {
        ret = false;
    } else if (!CopyToString(tb, formatOutput)) {
        errMsg = "[E003009] Internal error: Invalid tlv buffer";
    } else {
        ret = true;
    }
    delete[](reinterpret_cast<char *>(argv));
    argv = nullptr;
    return ret;
}

bool CopyToString(const TlvBuf &tlvBuf, string &output)
{
    uint32_t bufSize = tlvBuf.GetBufSize();
    if (bufSize <= 0) {
        WRITE_LOG(LOG_FATAL, "tlvSize error, size is: %u", bufSize);
        return false;
    }
    uint8_t* tlvs = new uint8_t[bufSize];
    if (tlvs == nullptr) {
        WRITE_LOG(LOG_FATAL, "failed to allocate tlvs memory");
        return false;
    }
    bool ret = false;
    if (tlvBuf.CopyToBuf(tlvs, bufSize)) {
        output = string(reinterpret_cast<char *>(tlvs), bufSize);
        ret = true;
    } else {
        WRITE_LOG(LOG_FATAL, "tlvs CopyToString failed");
    }
    delete[] tlvs;
    tlvs = nullptr;
    return ret;
}

bool ParameterToTlv(char **argv, int argc, TlvBuf &tlvBuf, string &errMsg)
{
    int skipNext = 0;
    const string cmdOptionBundle = "-b";
    string shellCommand = "";
    bool ret = false;
    for (int i = 0; i < argc; i++) {
        if (skipNext > 0) {
            skipNext--;
            continue;
        }
        if (std::strcmp(argv[i], cmdOptionBundle.c_str()) == 0) {
            if (i + 1 >= argc) {
                WRITE_LOG(LOG_FATAL, "empty bundle name");
                errMsg = "[E003005] The parameter is missing, correct your input by referring below:\n"
                         "Usage: hdc shell [-b bundlename] [COMMAND...]";
                break;
            }
            string bundlePath = argv[i + 1];
            if (!TlvAppendParameter(TAG_SHELL_BUNDLE, bundlePath, errMsg, tlvBuf)) {
                break;
            } else {
                skipNext++; // skip the next parameter
            }
        } else if (strlen(argv[i]) >= 1 && argv[i][0] == '-') {
            WRITE_LOG(LOG_FATAL, "Unsupport shell option: %s", argv[i]);
            errMsg = "[E003003] Unsupport shell option: " + std::string(argv[i]);
            break;
        } else {
            shellCommand = ConstructShellCommand(argv, i, argc);
            if (!TlvAppendParameter(TAG_SHELL_CMD, shellCommand, errMsg, tlvBuf)) {
                break;
            }
            ret = true;
            break;
        }
    }
    if (errMsg.empty() && shellCommand.empty()) {
        errMsg = "[E003002] Unsupport interactive shell command option";
    }
    return ret;
}

bool TlvAppendParameter(const uint32_t tag, const string &shellCommand, string &errMsg, TlvBuf &tlvBuf)
{
    if (tag == TAG_SHELL_CMD && shellCommand.empty()) {
        errMsg = "[E003002] Unsupport interactive shell command option";
        return false;
    }
    if (tag == TAG_SHELL_BUNDLE && !Base::CheckBundleName(shellCommand)) {
        errMsg = "[E003001] Invalid bundle name: " + shellCommand;
        return false;
    }
    if (!tlvBuf.Append(tag, shellCommand.size(),
        reinterpret_cast<uint8_t *>(const_cast<char *>(shellCommand.c_str())))) {
        errMsg = "[E003008] Internal error: Failed to add value to TLV buffer";
        return false;
    }
    return true;
}

string ConstructShellCommand(char **argv, const int startIndex, const int argc)
{
    if (argv == nullptr || startIndex >= argc) {
        return "";
    }
    string shellCommand;
    for (int j = startIndex; j < argc; j++) {
        shellCommand += argv[j];
        if (j != argc - 1) {
            shellCommand += " ";
        }
    }
    return shellCommand;
}
}
}
