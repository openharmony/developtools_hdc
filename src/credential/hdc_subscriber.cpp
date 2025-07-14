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
#include "hdc_subscriber.h"

using namespace Hdc;
using namespace OHOS::AccountSA;

void HdcSubscriber::OnAccountsChanged(const OHOS::AccountSA::OsAccountStateData& data)
{
    WRITE_LOG(LOG_INFO, "Recove data.state:%d", data.state);
    std::string path = std::string("/data/service/el1/public/hdc_server/") +
        std::to_string(data.toId);
    mode_t mode = (S_IRWXU | S_IRWXG | S_IXOTH | S_ISGID);

    switch(data.state) {
        case OsAccountState::CREATED:
            if (::mkdir(path.c_str(), mode) != 0) {
                WRITE_LOG(LOG_FATAL, "Failed to create directory ,error is :%s", strerror(errno));
                return;
            }
            if (::chmod(path.c_str(), mode) != 0) {
                WRITE_LOG(LOG_FATAL, "Failed to set directory permissions, error is :%s", strerror(errno));
                return;
            }
            WRITE_LOG(LOG_DEBUG, "Directory created successfully: %s", path.c_str());
            break;
        case OsAccountState::REMOVED:
            if (!Base::RemovePath(path.c_str())) {
                WRITE_LOG(LOG_DEBUG, "Directory removed successfully: %s", path.c_str());
            } else {
                WRITE_LOG(LOG_FATAL, "Failed to remove directory: %s", path.c_str());
            }
            break;
        default:
            WRITE_LOG(LOG_DEBUG, "This state is not support,state is:%d", data.state);
            break;
    }
    if (data.callback != nullptr) {
        data.callback->OnComplete(); // 执行握手回调
    }
    return;
}

void HdcSubscriber::OnAccountsChanged(const int& id)
{
}

int HdcAccountSubscriberMonitor()
{
    std::set<OsAccountState> states = { OsAccountState::CREATED, OsAccountState::REMOVED }
    OsAccountSubscribeInfo subscribeInfo(states, false);
    auto subscriber = std::make_shared<HdcSubscriber>(subscribeInfo);

    const int MAX_RETRY = 10;
    int retryCount = 0;

    while (retryCount < MAX_RETRY &&
        !OsAccountManager::SubscribeOsAccount(subscriber)) {
        ++retryCount;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        WRITE_LOG(LOG_FATAL, "SubscribeOsAccount failed, %d/%d", retryCount, MAX_RETRY);
    }
    
    if (retryCount < MAX_RETRY) {
        WRITE_LOG(LOG_DEBUG, "SubscribeOsAccount success");
    } else {
        WRITE_LOG(LOG_FATAL, "SubscribeOsAccount failed after %d retries", MAX_RETRY);
    }
    return 0;
}