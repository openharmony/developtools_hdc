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
#include "credential_base.h"
#include "hdc_subscriber.h"

using namespace Hdc;
using namespace OHOS::AccountSA;

void HdcSubscriber::OnStateChanged(const OHOS::AccountSA::OsAccountStateData& data)
{
    WRITE_LOG(LOG_INFO, "Recv data.state:%d, data.toId:%d", data.state, data.toId);
    std::string strId = std::to_string(data.toId);
    std::string path = std::string("/data/service/el1/public/hdc_server/") +
        strId;
    mode_t mode = (S_IRWXU | S_IRWXG | S_IXOTH | S_ISGID);

    switch (data.state) {
        case OsAccountState::CREATED:
            if (::mkdir(path.c_str(), mode) != 0) {
                WRITE_LOG(LOG_FATAL, "Failed to create directory ,error is :%s", strerror(errno));
                break;
            }
            if (::chmod(path.c_str(), mode) != 0) {
                WRITE_LOG(LOG_FATAL, "Failed to set directory permissions, error is :%s", strerror(errno));
                break;
            }
            WRITE_LOG(LOG_DEBUG, "Directory created successfully.");
            break;
        case OsAccountState::REMOVED:
            if (RemovePath(path.c_str()) == 0) {
                WRITE_LOG(LOG_DEBUG, "Directory removed successfully.");
            } else {
                WRITE_LOG(LOG_FATAL, "Failed to remove directory, error is:%s", strerror(errno));
            }
            break;
        default:
            WRITE_LOG(LOG_FATAL, "This state is not support,state is:%d", data.state);
            break;
    }
    return;
}

void HdcSubscriber::OnAccountsChanged(const int& id)
{
}

int HdcAccountSubscriberMonitor()
{
    static std::shared_ptr<HdcSubscriber> subscriber;
    std::set<OsAccountState> states = { OsAccountState::CREATED, OsAccountState::REMOVED };
    OsAccountSubscribeInfo subscribeInfo(states, false);
    subscriber = std::make_shared<HdcSubscriber>(subscribeInfo);

    const int maxRetry = 10;
    int retryCount = 0;

    while (retryCount < maxRetry &&
        (OsAccountManager::SubscribeOsAccount(subscriber) != 0)) {
        ++retryCount;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        WRITE_LOG(LOG_FATAL, "SubscribeOsAccount failed, %d/%d", retryCount, maxRetry);
    }
    
    if (retryCount < maxRetry) {
        WRITE_LOG(LOG_DEBUG, "SubscribeOsAccount success.");
    } else {
        WRITE_LOG(LOG_FATAL, "SubscribeOsAccount failed after %d retries.", maxRetry);
        return -1;
    }

    return 0;
}