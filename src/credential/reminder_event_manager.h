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
#ifndef HDC_REMINDER_EVENT_MANAGER_H
#define HDC_REMINDER_EVENT_MANAGER_H

#include "common.h"
#include "common_event_manager.h"
#include "common_event_support.h"

class ReminderEventManager {
public:
    ReminderEventManager();
    void Init();
    class ReminderEventSubscriber : public OHOS::EventFwk::CommonEventSubscriber {
    public:
        explicit ReminderEventSubscriber(const OHOS::EventFwk::CommonEventSubscribeInfo &subscriberInfo);
        virtual void OnReceiveEvent(const OHOS::EventFwk::CommonEventData &data);
    };
    std::shared_ptr<ReminderEventSubscriber> customSubscriber;
};

#endif // HDC_REMINDER_EVENT_MANAGER_H