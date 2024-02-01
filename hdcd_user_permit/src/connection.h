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

#ifndef HEAD_CONECTION_HEAD
#define HEAD_CONECTION_HEAD

#include "iremote_stub.h"
#include "iremote_object.h"
#include "ability_connect_callback_stub.h"
#include <semaphore.h>

namespace OHOS {
namespace HDC {
namespace AUTH {

using namespace std;

class HdcdConnection : public AAFwk::AbilityConnectionStub {
public:
    HdcdConnection()
    {
        sem_init(&sem, 0, 0);
        showDialogResult = false;
    }
    virtual ~HdcdConnection() = default;

    void OnAbilityConnectDone(const AppExecFwk::ElementName &element, const sptr<IRemoteObject> &remoteObject,
        int32_t resultCode) override;
    void OnAbilityDisconnectDone(const AppExecFwk::ElementName &element, int32_t resultCode) override;
    bool GetSettingBundleName(string &bundle);
    bool GetShowDialogResult(void);
private:
    sem_t sem;
    bool showDialogResult;
};

} // namespace AUTH
} // namespace HDC
} // namespace OHOS
#endif // HEAD_CONECTION_HEAD

