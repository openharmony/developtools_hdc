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

#ifndef HDC_RUNTIME_CONFIG_H
#define HDC_RUNTIME_CONFIG_H

#include <string>

namespace Hdc {

class RuntimeConfig {
public:
    static RuntimeConfig& Instance();

    bool isServerMode = false;
    bool isPullServer = true;
    bool isPcDebugRun = false;
    bool isTCPorUSB = false;
    bool isCustomLoglevel = false;
    bool externalCmd = false;
    int isTestMethod = 0;
    std::string connectKey = "";
    std::string serverListenString = "";
    std::string containerInOut = "";

    // for subserver feature
    bool isSubserver = false;
    std::string subserverPort = "";
    std::string subserverSerial = "";
    std::string subserverLogFileName = "";

private:
    RuntimeConfig() = default;
    ~RuntimeConfig() = default;
    RuntimeConfig(const RuntimeConfig&) = delete;
    RuntimeConfig& operator=(const RuntimeConfig&) = delete;
};

} // namespace Hdc

#endif // HDC_RUNTIME_CONFIG_H
