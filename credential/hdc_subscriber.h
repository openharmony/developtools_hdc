/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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
#ifndef HDC_SUBSCRIBER_H
#define HDC_SUBSCRIBER_H

#include <regex>

#include "common.h"
#include "os_account_manager.h"
#include "os_account_subscriber.h"

class HdcSubscriber : public OHOS::AccountSA::OsAccountSubscriber {
public:
    HdcSubscriber(const OHOS::AccountSA::OsAccountSubscribeInfo& info) : OHOS::AccountSA::OsAccountSubscriber(info){};

    void OnStateChanged(const OHOS::AccountSA::OsAccountStateData& data) override;
    void OnAccountsChanged(const int& id) override;
};

int HdcAccountSubscriberMonitor();

#endif // HDC_SUBSCRIBER_H