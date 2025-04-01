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

#ifndef SUDO_IAM_H
#define SUDO_IAM_H
#include <condition_variable>
#include <mutex>
#include "account_iam_client.h"
#include "account_iam_info.h"
#include "account_iam_client_callback.h"
#include "i_inputer.h"

extern std::condition_variable g_condVarForAuth;
extern bool g_authFinish;
extern std::mutex g_mutexForAuth;

namespace OHOS {
namespace UserIam {
namespace PinAuth {

class SudoIInputer : public IInputer {
public:
    virtual ~SudoIInputer();
    void OnGetData(int32_t authSubType, std::vector<uint8_t> challenge,
                                std::shared_ptr<IInputerData> inputerData) override;
    void SetPasswd(char *pwd, int len);
private:
    std::vector<uint8_t> passwd_;
};

} //PinAuth
} //UserIam

namespace AccountSA {

class SudoIDMCallback : public IDMCallback {
public:
    virtual ~SudoIDMCallback() = default;
    SudoIDMCallback();
    void OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo) override;
    void OnResult(int32_t result, const Attributes &extraInfo) override;
    bool GetVerifyResult(void);
private:
    bool verifyResult_;
};
} // AccountSA
} //OHOS
#endif //SUDO_IAM_H
