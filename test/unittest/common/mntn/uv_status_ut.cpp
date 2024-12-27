/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "mntn/uv_status.h"
#include <random>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing::ext;
using namespace testing;

namespace Hdc {
class HandleTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
private:
    std::vector<uintptr_t> BuildHandles(LoopStatus &loop, const int num, const uintptr_t handle, const string &name, const uint64_t duration, const uint64_t bytes)
    {
        std::vector<uintptr_t> handles;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distr(0x400000, 0x40000000);
        for (int i = 0; i < num; i++) {
            int r = distr(gen);
            uintptr_t hr = handle + r;
            loop.AddHandle(hr, name + std::to_string(hr));
            handles.push_back(hr);
        }
        return handles;
    }
    void RandExecuteHandles(LoopStatus &loop, std::vector<uintptr_t> handles)
    {
        for (uintptr_t handle : handles) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> distr(0x400000, 0x40000000);
            int r = distr(gen);
            ssize_t bytes = r % (512 * 1024);
            loop.HandleStart(handle, bytes);
            usleep(r % 100);
            if (r % 3 != 0) {
                loop.HandleEnd(handle);
            }
        }
    }
    void BuildHandle(LoopStatus &loop, const uintptr_t handle, const string &name, const uint64_t duration, const uint64_t bytes)
    {
        std::vector<uintptr_t> handles = BuildHandles(loop, 100, handle, name, duration, bytes);
        for (int i = 0; i < 1000; i++) {
            RandExecuteHandles(loop, handles);
        }
    }
};

void HandleTest::SetUpTestCase()
{
}

void HandleTest::TearDownTestCase() {}

void HandleTest::SetUp() {}

void HandleTest::TearDown() {}

HWTEST_F(HandleTest, Handle_Constructor_001, TestSize.Level0)
{
    {
        LoopStatus loop1("ut-loop");
        loop1.Init();
        BuildHandle(loop1, 1, "handle-1-", 100, 200);
        loop1.Close();
        LoopStatus loop2("ut-loop");
        loop2.Init();
        BuildHandle(loop2, 1, "handle-2-", 100, 200);
        loop2.Close();
    }
    ASSERT_EQ(0, 0);
}

} // namespace Hdc
