/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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
#include "runtime_cfg_wrapper.h"
#include <securec.h>
#include <cstring>
#include <mutex>
#include <string>

namespace {
enum CfgField {
    CFG_SERVER_MODE = 0,
    CFG_PULL_SERVER,
    CFG_PC_DEBUG,
    CFG_TCP_OR_USB,
    CFG_CUSTOM_LOG,
    CFG_EXTERNAL_CMD,
    CFG_TEST_METHOD,
    CFG_IS_SUBSERVER,
    CFG_CONNECT_KEY,
    CFG_SERVER_LISTEN,
    CFG_CONTAINER,
    CFG_SUBSERVER_PORT,
    CFG_SUBSERVER_SERIAL,
    CFG_SUBSERVER_LOG,
    CFG_MAX,
};

struct RuntimeCfg {
    bool bools[8] = {false, true, false, false, false, false, false, false};
    int testMethod = 0;
    std::string strings[6];
};

std::mutex g_mutex;
RuntimeCfg g_cfg;

bool IsBoolField(int fieldId)
{
    return fieldId >= CFG_SERVER_MODE && fieldId <= CFG_IS_SUBSERVER;
}

bool IsStringField(int fieldId)
{
    return fieldId >= CFG_CONNECT_KEY && fieldId <= CFG_SUBSERVER_LOG;
}
}

extern "C" {
void RtCfgSetBool(int fieldId, bool val)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (IsBoolField(fieldId)) {
        g_cfg.bools[fieldId] = val;
    }
}

bool RtCfgGetBool(int fieldId)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (IsBoolField(fieldId)) {
        return g_cfg.bools[fieldId];
    }
    return false;
}

void RtCfgSetString(int fieldId, const char *val)
{
    if (val == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (IsStringField(fieldId)) {
        g_cfg.strings[fieldId - CFG_CONNECT_KEY] = val;
    }
}

int RtCfgGetString(int fieldId, char *out, int maxLen)
{
    if (out == nullptr || maxLen <= 0) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (IsStringField(fieldId)) {
        const std::string &s = g_cfg.strings[fieldId - CFG_CONNECT_KEY];
        if (static_cast<int>(s.size()) >= maxLen) {
            return -1;
        }
        if (strncpy_s(out, maxLen, s.c_str(), s.size()) != EOK) { return -1; }
        return static_cast<int>(s.size());
    }
    return -1;
}

void RtCfgSetInt(int fieldId, int val)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (fieldId == CFG_TEST_METHOD) {
        g_cfg.testMethod = val;
    }
}

int RtCfgGetInt(int fieldId)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (fieldId == CFG_TEST_METHOD) {
        return g_cfg.testMethod;
    }
    return 0;
}
}
