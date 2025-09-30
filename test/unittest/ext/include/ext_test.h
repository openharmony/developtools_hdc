/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef HDC_EXT_TEST_H
#define HDC_EXT_TEST_H
#include <gtest/gtest.h>
#include <fstream>
#include <sys/stat.h>
#include <chrono>
#include "base.h"
#include "circle_buffer.h"
#include "compress.h"
#include "credential_message.h"
#include "decompress.h"
#include "header.h"
#include "heartbeat.h"
#include "server_cmd_log.h"
#include "auth.h"
#include "entry.h"
namespace Hdc {
class HdcExtTest : public ::testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
    void RemoveTestFiles();
protected:
    CircleBuffer* buffer;
    Compress* compress;
    CredentialMessage* message;
    Decompress* decompress;
    Entry* entry;
    Header* header;
    HdcHeartbeat* heartbeat;
    ServerCmdLog* serverCmdLog;
    std::string testKeyPath;
    std::string testPubKeyPath;
};
}   // namespace Hdc
#endif  // HDC_EXT_TEST_H