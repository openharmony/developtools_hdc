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
#include <filesystem>
#include "credential_base.h"

using namespace Hdc;
using namespace OHOS::AccountSA;
namespace fs = std::filesystem;

namespace {
static const std::string USER_DIR_PREFIX_PATH = "/data/service/el1/public/hdc_server/";
static const mode_t MODE = (S_IRWXU | S_IRWXG | S_IXOTH | S_ISGID);
}

void HdcSubscriber::OnStateChanged(const OHOS::AccountSA::OsAccountStateData& data)
{
    WRITE_LOG(LOG_INFO, "Recv data.state:%d, data.toId:%d", data.state, data.toId);
    std::string path = USER_DIR_PREFIX_PATH + std::to_string(data.toId);
    switch (data.state) {
        case OsAccountState::CREATED:
            if (HdcCredentialBase::CreatePathWithMode(path.c_str(), MODE) != 0) {
                WRITE_LOG(LOG_FATAL, "Failed to create directory, error is:%s", strerror(errno));
            }
            break;
        case OsAccountState::REMOVED:
            if (HdcCredentialBase::RemovePath(path) != 0) {
                WRITE_LOG(LOG_FATAL, "Failed to remove directory, error is:%s", strerror(errno));
            }
            break;
        default:
            WRITE_LOG(LOG_FATAL, "This state is not support,state is:%d", data.state);
            break;
    }
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

void FreshAccountsPath()
{
    std::vector<OHOS::AccountSA::OsAccountInfo> osAccountInfos;
    OHOS::ErrCode err = OHOS::AccountSA::OsAccountManager::QueryAllCreatedOsAccounts(osAccountInfos);
    if (err != 0) {
        WRITE_LOG(LOG_FATAL, "QueryAllCreatedOsAccounts failed, error is:%d", err);
        return;
    }
    std::vector<std::string> existUserIds;
    for (const auto& info : osAccountInfos) {
        existUserIds.push_back(std::to_string(info.GetLocalId()));
    }
    std::vector<std::string> existUserDirs;
    for (const auto& entry : fs::directory_iterator(USER_DIR_PREFIX_PATH)) {
        std::string dir = entry.path().filename().string();
        if (HdcCredentialBase::IsUserDir(dir)) {
            existUserDirs.push_back(dir);
        }
    }
    std::vector<std::string> needCreate = HdcCredentialBase::Substract<std::string>(existUserIds, existUserDirs);
    std::vector<std::string> needRemove = HdcCredentialBase::Substract<std::string>(existUserDirs, existUserIds);
    for (const auto& item : needCreate) {
        std::string path = USER_DIR_PREFIX_PATH + item;
        if (!HdcCredentialBase::CreatePathWithMode(path.c_str(), MODE)) {
            WRITE_LOG(LOG_FATAL, "Failed to create directory, error is:%s", strerror(errno));
        }
    }
    for (const auto& item : needRemove) {
        std::string path = USER_DIR_PREFIX_PATH + item;
        if (HdcCredentialBase::RemovePath(path.c_str()) != 0) {
            WRITE_LOG(LOG_FATAL, "Failed to remove directory, error is:%s", strerror(errno));
        }
    }
}