/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#ifndef HDC_DAEMON_UNITY_H
#define HDC_DAEMON_UNITY_H
#include "daemon_common.h"

namespace Hdc {
class HdcDaemonUnity : public HdcTaskBase {
public:
    explicit HdcDaemonUnity(HTaskInfo hTaskInfo);
    ~HdcDaemonUnity() override;
    bool CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize) override;
    void StopTask() override;
    bool ReadyForRelease() override;

private:
    static void OnFdRead(uv_fs_t *req);
    int ExecuteShell(const string &shellCommand, string optionPath = "");
    bool FindMountDeviceByPath(const char *toQuery, char *dev);
    bool RemountPartition(const char *dir);
    bool CallRemount();
    bool RemountDevice();
    bool RebootDevice(const string &cmd);
    bool SetDeviceRunMode(const char *cmd);
    bool GetHiLog(const char *cmd);
    bool ListJdwpProcess(void *daemonIn);
    bool AsyncCmdOut(bool finish, const string result);
    bool TrackJdwpProcess(void *daemonIn, const string& param);
    void RemoveJdwpTracker();
    int ExecuteShellExtend(const uint8_t *payload, const int payloadSize);
    int ExecuteOptionShell(const string &shellCommand, const string &bundleName);
    bool CheckbundlePath(const string &bundleName, string &mountPath);
    AsyncCmd asyncCommand;
    uint16_t currentDataCommand;
#ifdef UNIT_TEST
    int countUt = 0;
#endif
};
}  // namespace Hdc
#endif  // HDC_DAEMON_UNITY_H
