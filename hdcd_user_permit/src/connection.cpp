/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. Allrights reserved.
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
#include "connection.h"
#include "extension_manager_proxy.h"
#include <semaphore.h>

namespace OHOS {
namespace HDC {
namespace AUTH {

using namespace std;
using namespace AAFwk;

void HdcdConnection::OnAbilityConnectDone(const AppExecFwk::ElementName &element,
    const sptr<IRemoteObject> &remoteObject, int32_t resultCode)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    const int paramNum = 3;
    string bundleName = "com.ohos.settings";
    string abilityName = "USBDebugDialog";
    string parameters = "{\"ability.want.params.uiExtensionType\":\"sysDialog/common\",\"sysDialogZOrder\":2}";

    AUTH_LOGE("connect success");

    data.WriteInt32(paramNum);
    data.WriteString16(Str8ToStr16("bundleName"));
    data.WriteString16(Str8ToStr16(bundleName));
    data.WriteString16(Str8ToStr16("abilityName"));
    data.WriteString16(Str8ToStr16(abilityName));

    data.WriteString16(Str8ToStr16("parameters"));
    data.WriteString16(Str8ToStr16(parameters));

    if (!data.WriteParcelable(&element)) {
        AUTH_LOGE("Connect done element error");
        return;
    }
    if (!data.WriteRemoteObject(remoteObject)) {
        AUTH_LOGE("Connect done remote object error");
        return;
    }
    if (!data.WriteInt32(resultCode)) {
        AUTH_LOGE("Connect done result code error");
        return;
    }

    int32_t ret = remoteObject->SendRequest(IAbilityConnection::ON_ABILITY_CONNECT_DONE, data, reply, option);
    showDialogResult = (ret == 0);
    sem_post(&sem);
    AUTH_LOGE("SendRequest(bundle(%s)ability(%s)parameters(%s)) ret(%d)",
              bundleName.c_str(), abilityName.c_str(), parameters.c_str(), ret);
}

bool HdcdConnection::GetShowDialogResult(void)
{
    AUTH_LOGE("wait for dialog ability");
    sem_wait(&sem);
    sem_destroy(&sem);
    AUTH_LOGE("wait dialog ability over");
    return showDialogResult;
}

void HdcdConnection::OnAbilityDisconnectDone(const AppExecFwk::ElementName &element,
    int32_t resultCode)
{
    AUTH_LOGE("disconnect success");
}

} // namespace AUTH
} // namespace HDC
} // namespace OHOS

