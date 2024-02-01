/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef REGISTER_HDC_CONNECT_H
#define REGISTER_HDC_CONNECT_H

#include <string>
#include "define_register.h"
namespace Hdc {
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */
void StartConnect(const std::string& processName, const std::string& pkgName, bool isDebug, Callback cb);
void StopConnect();
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

class ConnectManagement {
public:
    ConnectManagement() = default;
    ~ConnectManagement() {}
    void SetProcessName(const std::string& processName);
    std::string GetProcessName() const;
    void SetPkgName(const std::string& pkgName);
    std::string GetPkgName() const;
    void SetDebug(bool isDebug);
    bool GetDebug() const;
    void SetCallback(Callback cb);
    Callback GetCallback() const;

private:
    std::string processName_;
    std::string pkgName_;
    bool isDebug_;
    Callback cb_;
};
} // namespace Hdc

#endif // REGISTER_HDC_CONNECT_H
