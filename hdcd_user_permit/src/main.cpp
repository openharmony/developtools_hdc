/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "common.h"
#include "want.h"
#include "parameter.h"
#include "connection.h"
#include "ipc_skeleton.h"
#include "scene_board_judgement.h"
#include "extension_manager_client.h"

using namespace std;
using namespace OHOS;
using namespace OHOS::HDC::AUTH;
using namespace OHOS::AAFwk;

static HdcdConnection *ConnectExtensionAbility(void)
{
    Want want;
    string bundle;
    string ability;

    auto con = new HdcdConnection();
    if (!con) {
        AUTH_LOGE("alloc mem failed");
        return nullptr;
    }

    if (Rosen::SceneBoardJudgement::IsSceneBoardEnabled()) {
        bundle = "com.ohos.sceneboard";
        ability = "com.ohos.sceneboard.systemdialog";
    } else {
        bundle = "com.ohos.systemui";
        ability = "com.ohos.systemui.dialog";
    }
    want.SetElementName(bundle, ability);

    std::string identity = IPCSkeleton::ResetCallingIdentity();
    ErrCode ret = ExtensionManagerClient::GetInstance().ConnectServiceExtensionAbility(want, con, nullptr, -1);
    IPCSkeleton::SetCallingIdentity(identity);

    AUTH_LOGI("connect bundle(%s) ability(%s) ret(%d)", bundle.c_str(), ability.c_str(), ret);

    return (ret == ERR_OK) ? con : nullptr;
}

static bool WaitDialogResult(void)
{
    if (WaitParameter("persist.hdc.daemon.auth_result", "auth_result:*", WAIT_USER_PERMIT_TIMEOUT) != 0) {
        AUTH_LOGE("wait user permit failed");
        return false;
    }
    AUTH_LOGE("wait user permit over");
    return true;
}

static int GetUserPermit(void)
{
    auto con = ConnectExtensionAbility();
    if (!con) {
        AUTH_LOGE("connect ability failed");
        return USER_PERMIT_ERR_CON_ABILITY_FAIL;
    }
    if (!con->GetShowDialogResult()) {
        AUTH_LOGE("show dialog failed");
        return USER_PERMIT_ERR_SHOW_DIALOG_FAIL;
    }
    if (!WaitDialogResult()) {
        AUTH_LOGE("wait ability result failed");
        return USER_PERMIT_ERR_WAIT_DIALOG_FAIL;
    }

    return USER_PERMIT_SUCCESS;
}

int main(int argc, char** argv)
{
    // ohos not support, direct success
    if (!Rosen::SceneBoardJudgement::IsSceneBoardEnabled()) {
        if (SetParameter("persist.hdc.daemon.auth_result", "auth_result:2") != 0) {
            AUTH_LOGW("set auth_result failed");
            return USER_PERMIT_ERR_SET_AUTH_RESULT_FAIL;
        }

        AUTH_LOGF("always forever permit for ohos");
        return USER_PERMIT_SUCCESS;
    }

    return GetUserPermit();
}
