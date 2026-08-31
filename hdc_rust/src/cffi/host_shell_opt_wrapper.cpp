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
#include "host_shell_opt_wrapper.h"
#include "binary_tlv.h"
#include <cctype>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {
constexpr size_t BUNDLE_MIN = 7;
constexpr size_t BUNDLE_MAX = 128;
const char *BUNDLE_OPT = "-b";

std::vector<std::string> SplitArgs(const std::string &input)
{
    std::vector<std::string> args;
    std::string current;
    bool inQuote = false;
    for (char c : input) {
        if (c == '"') {
            inQuote = !inQuote;
        } else if ((c == ' ' || c == '\t' || c == '\n') && !inQuote) {
            if (!current.empty()) {
                args.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        args.push_back(current);
    }
    return args;
}

std::string ConstructShellCmd(const std::vector<std::string> &argv, size_t start)
{
    std::string result;
    for (size_t i = start; i < argv.size(); i++) {
        if (i > start) {
            result += " ";
        }
        result += argv[i];
    }
    return result;
}

int TlvAppendParam(void *tlv, uint32_t tag, const std::string &val, std::string &errMsg)
{
    if (tag == SHELL_TAG_CMD && val.empty()) {
        errMsg = "[E003002] Unsupport interactive shell command option";
        return -1;
    }
    if (tag == SHELL_TAG_BUNDLE && ShellOptCheckBundleName(val.c_str()) == 0) {
        errMsg = "[E003001] Invalid bundle name: " + val;
        return -1;
    }
    if (!TlvAppend(tlv, tag, reinterpret_cast<const uint8_t *>(val.c_str()), val.size())) {
        errMsg = "[E003008] Internal error: Failed to add value to TLV buffer";
        return -1;
    }
    return 0;
}
}

extern "C" {
int ShellOptCheckBundleName(const char *name)
{
    if (name == nullptr) {
        return 0;
    }
    size_t len = strlen(name);
    if (len < BUNDLE_MIN || len > BUNDLE_MAX) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_') {
            return 0;
        }
    }
    return 1;
}

int ParseShellParams(const std::vector<std::string> &argv, void *tlv,
                     bool &hasCommand)
{
    bool skipNext = false;
    bool success = false;
    std::string errMsg;

    for (size_t i = 0; i < argv.size(); i++) {
        if (skipNext) {
            skipNext = false;
            continue;
        }
        if (argv[i] == BUNDLE_OPT) {
            if (i + 1 >= argv.size()) {
                return -1;
            }
            int ret = TlvAppendParam(tlv, SHELL_TAG_BUNDLE, argv[i + 1], errMsg);
            if (ret == 0) {
                success = true;
                skipNext = true;
            } else {
                return -1;
            }
        } else if (!argv[i].empty() && argv[i][0] == '-') {
            return -1;
        } else {
            std::string cmd = ConstructShellCmd(argv, i);
            int ret = TlvAppendParam(tlv, SHELL_TAG_CMD, cmd, errMsg);
            if (ret == 0) {
                success = true;
            } else {
                return -1;
            }
            break;
        }
    }
    hasCommand = (TlvFind(tlv, SHELL_TAG_CMD, nullptr, nullptr, 0) != 0);
    return success ? 0 : -1;
}

int CopyTlvToOutput(void *tlv, uint8_t *out, int maxOut)
{
    uint32_t size = TlvGetSize(tlv);
    if (size == 0 || static_cast<int>(size) > maxOut) {
        return -1;
    }
    if (!TlvCopyToBuf(tlv, out, size)) {
        return -1;
    }
    return static_cast<int>(size);
}
}

extern "C" {
int ShellOptCheckBundleName(const char *name)
{
    if (name == nullptr) {
        return 0;
    }
    size_t len = strlen(name);
    if (len < BUNDLE_MIN || len > BUNDLE_MAX) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_') {
            return 0;
        }
    }
    return 1;
}

int ShellOptFormatToTlv(const char *params, int startPos,
                        uint8_t *out, int maxOut, int *hasCommand)
{
    if (params == nullptr || startPos < 0 || out == nullptr) {
        return -1;
    }
    if (startPos >= static_cast<int>(strlen(params))) {
        return -1;
    }
    std::string sub = std::string(params + startPos);
    std::vector<std::string> argv = SplitArgs(sub);
    if (argv.empty()) {
        return -1;
    }

    void *tlv = TlvCreate();
    if (tlv == nullptr) {
        return -1;
    }

    bool cmdFound = false;
    int parseResult = ParseShellParams(argv, tlv, cmdFound);
    if (parseResult != 0) {
        TlvDestroy(tlv);
        return -1;
    }

    if (hasCommand != nullptr) {
        *hasCommand = cmdFound ? 1 : 0;
    }

    int result = CopyTlvToOutput(tlv, out, maxOut);
    TlvDestroy(tlv);
    return result;
}
}
