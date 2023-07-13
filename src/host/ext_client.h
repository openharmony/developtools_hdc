/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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
#ifndef HDC_EXT_CLIENT_H
#define HDC_EXT_CLIENT_H

#include "host_common.h"

extern std::map<std::string, std::string> g_lists;
extern bool g_show;

namespace Hdc {
class ExtClient {
public:
    std::string connectKey;
    std::string containerInOut;
    ExtClient();
    virtual ~ExtClient();
    void ExecuteCommand(const string &command);
    bool Init();
    static bool SharedLibraryExist();

private:
    uv_lib_t lib;
    void Version(const std::string &str);
    void Help(const std::string &str);
    void Discover(const std::string &str);
    void Start(const std::string &str);
    void Kill(const std::string &str);
    void ListTargets(const std::string &str);
    void Connect(const std::string &str);
    void Shell(const std::string &str);
    void File(const std::string &str);
    void Fport(const std::string &str);
    void Rport(const std::string &str);
    void Install(const std::string &str);
    void Uninstall(const std::string &str);
    void Jpid(const std::string &str);
    void TrackJpid(const std::string &str);
    void Utility(const std::string &str);
    void Bugreport(const std::string &str);
    void WaitFor(const std::string &str);
    void UnknowCommand(const std::string &str);
    std::string RemoveRemoteCwd(const std::string &str);
    void UpdateList(const std::string &str);
    static std::string HandleLib(const std::string &str, const char *name, uv_lib_t &lib);
    std::string Handle(const std::string &str, const char *name);
    string WithConnectKey(const string &str);
    static void WaitForExtent(const std::string &str);
    static string GetPath();
    static void RegistExecFunc(uv_lib_t *lib);
};
}
#endif
