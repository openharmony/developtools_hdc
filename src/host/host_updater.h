/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#ifndef HDC_HOST_UPDATER_H
#define HDC_HOST_UPDATER_H

#include "transfer.h"
#include "translate.h"

namespace Hdc {
class HostUpdater : public HdcTransferBase {
public:
    explicit HostUpdater(HTaskInfo hTaskInfo);
    virtual ~HostUpdater();
    bool CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize) override;
    static bool CheckMatchUpdate(const std::string &input, TranslateCommand::FormatCommand &outCmd);
    static bool ConfirmCommand(const string &commandIn, bool &closeInput);

protected:
    void CheckMaster(CtxFile *context) override;

private:
    bool BeginTransfer(const std::string &function, const uint8_t *payload, int payloadSize, size_t minParam,
    size_t fileIndex);
    bool CheckUpdateContinue(const uint16_t command, const uint8_t *payload, int payloadSize);
    void RunQueue(CtxFile &context);
    void ProcessProgress(uint8_t percentage);
    void SendRawData(std::string rawData) const;
    bool CheckCmd(HdcCommand command, uint8_t *payload, int payloadSize, size_t paramCount);

private:
    bool sendProgress_ = false;
};
}
#endif