/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
#include "reminder_event_manager.h"

using namespace Hdc;
using namespace OHOS;

ReminderEventManager::ReminderEventSubscriber::ReminderEventSubscriber(
    const EventFwk::CommonEventSubscribeInfo &subscriberInfo)
    : CommonEventSubscriber(subscriberInfo)
{
}

void ReminderEventManager::ReminderEventSubscriber::OnReceiveEvent(
    const EventFwk::CommonEventData &data)
{
    AAFwk::Want want = data.GetWant();
    std::string action = want.GetAction();
    std::string accountUserId = std::to_string(data.GetCode());
    std::string path = std::string("/data/service/el1/public/hdc_server/") +
        accountUserId.c_str();
    mode_t mode = (S_IRWXU | S_IRWXG | S_IXOTH | S_ISGID);

    WRITE_LOG(LOG_DEBUG, "Recv Event is : %s", action.c_str());
    if (action == EventFwk::CommonEventSupport::COMMON_EVENT_USER_ADDED) {
        if (::mkdir(path.c_str(), mode) != 0) {
            WRITE_LOG(LOG_FATAL, "Failed to create directory ,error is :%s", strerror(errno));
            return;
        }
        if (::chmod(path.c_str(), mode) != 0) {
            WRITE_LOG(LOG_FATAL, "Failed to set directory permissions, error is :%s", strerror(errno));
            return;
        }
        WRITE_LOG(LOG_DEBUG, "Directory created successfully: %s", path.c_str());
    }
    if (action == EventFwk::CommonEventSupport::COMMON_EVENT_USER_REMOVED) {
        Base::RemovePath(path.c_str());
        WRITE_LOG(LOG_DEBUG, "Directory removed successfully: %s", path.c_str());
    }
    return;
}

ReminderEventManager::ReminderEventManager()
{
    Init();
}

void ReminderEventManager::Init()
{
    EventFwk::MatchingSkills customMatchingSkills;
    customMatchingSkills.AddEvent(EventFwk::CommonEventSupport::COMMON_EVENT_USER_ADDED);
    customMatchingSkills.AddEvent(EventFwk::CommonEventSupport::COMMON_EVENT_USER_REMOVED);

    EventFwk::CommonEventSubscribeInfo customSubscriberInfo(customMatchingSkills);
    customSubscriberInfo.SetThreadMode(EventFwk::CommonEventSubscribeInfo::COMMON);
    customSubscriber = std::make_shared<ReminderEventSubscriber>(customSubscriberInfo);

    const int MAX_RETRY = 10;
    int retryCount = 0;

    while (retryCount < MAX_RETRY &&
        !EventFwk::CommonEventManager::SubscribeCommonEvent(customSubscriber)) {
        ++retryCount;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        WRITE_LOG(LOG_FATAL, "SubscribeCommonEvent fail %d/%d", retryCount, MAX_RETRY);
    }

    if (retryCount < MAX_RETRY) {
        WRITE_LOG(LOG_DEBUG, "SubscribeCommonEvent success.");
    } else {
        WRITE_LOG(LOG_FATAL, "SubscribeCommonEvent failed after %d retries.", MAX_RETRY);
    }
    return;
}
