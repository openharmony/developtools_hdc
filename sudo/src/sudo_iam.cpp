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

#include "sudo_iam.h"
#include "account_iam_client.h"
#include "account_iam_info.h"
#include "account_iam_client_callback.h"
#include "i_inputer.h"

std::condition_variable g_condVarForAuth;
bool g_authFinish;
std::mutex g_mutexForAuth;

namespace OHOS {
namespace UserIam {
namespace PinAuth {

void SudoIInputer::OnGetData(int32_t authSubType, std::vector<uint8_t> challenge,
    std::shared_ptr<IInputerData> inputerData)
{
    inputerData->OnSetData(authSubType, passwd_);
}

void SudoIInputer::SetPasswd(char *pwd, int len)
{
    passwd_.resize(len);
    for (int i = 0; i < len; i++) {
        passwd_[i] = *pwd++;
    }
}

SudoIInputer::~SudoIInputer()
{
    for (int i = 0; i < (int)passwd_.size(); i++) {
        passwd_[i] = 0;
    }
}

} //PinAuth
} //UserIam

namespace AccountSA {

SudoIDMCallback::SudoIDMCallback()
{
    verifyResult_ = false;
}

void SudoIDMCallback::OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo)
{
    return;
}

void SudoIDMCallback::OnResult(int32_t result, const Attributes &extraInfo)
{
    verifyResult_ = false;
    if (result == 0) {
        verifyResult_ = true;
    }

    std::unique_lock<std::mutex> lock { g_mutexForAuth };
    g_authFinish = true;
    g_condVarForAuth.notify_all();
}

bool SudoIDMCallback::GetVerifyResult(void)
{
    return verifyResult_;
}

} // AccountSA
} //OHOS
