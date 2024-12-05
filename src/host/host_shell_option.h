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
#ifndef HOST_SHELL_OPTION_H
#define HOST_SHELL_OPTION_H

#include "host_common.h"

namespace Hdc {
namespace HostShellOption {
    string ConstructShellCommand(char **argv, const int startIndex, const int argc);
    bool ParameterToTlv(char **argv, int argc, TlvBuf &tlvBuf, string &errMsg);
    bool TlvAppendParameter(const uint32_t tag, const string &shellCommand, string &errMsg, TlvBuf &tlvBuf);
    bool FormatParametersToTlv(const string &parameters, const int startPos, string &formatOutput, string &errMsg);
    bool CopyToString(const TlvBuf &tlvBuf, string &output);
}
}
#endif // HOST_SHELL_OPTION_H
