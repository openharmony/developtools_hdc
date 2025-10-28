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
#include "i_inputer.h"
#include "user_auth_client_callback.h"

using namespace OHOS::UserIam::PinAuth;
using namespace OHOS::UserIam::UserAuth;

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

} // PinAuth

namespace UserAuth {
SudoIDMCallback::SudoIDMCallback()
{
    verifyResult_ = false;
}

void SudoIDMCallback::OnAcquireInfo(int32_t /* module */, uint32_t /* acquireInfo */, const Attributes& /* extraInfo */)
{
    return;
}

void SudoIDMCallback::OnResult(int32_t result, const Attributes &extraInfo)
{
    verifyResult_ = false;
    if (result == 0) {
        verifyResult_ = true;
    }
    if (!extraInfo.GetUint8ArrayValue(Attributes::ATTR_SIGNATURE, authToken_)) {
        verifyResult_ = false;
    }

    std::unique_lock<std::mutex> lock { g_mutexForAuth };
    g_authFinish = true;
    g_condVarForAuth.notify_all();
}

bool SudoIDMCallback::GetVerifyResult(void)
{
    return verifyResult_;
}

std::vector<uint8_t> SudoIDMCallback::GetAuthToken()
{
    return authToken_;
}

SudoIDMCallback::~SudoIDMCallback()
{
    authToken_.assign(authToken_.size(), 0);
}

} // UserAuth
} // UserIam
} // OHOS
