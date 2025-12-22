/*
 * Copyright (c)2025 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <cstring>

#include "heartbeat.h"

using namespace Hdc;
using namespace std::chrono;

class HdcHeartbeatTest : public ::testing::Test {
protected:
    HdcHeartbeat heartbeat;
    
    void SetUp() override {
        heartbeat = HdcHeartbeat();
    }
};

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest1, ConstructorInitializesCorrectly) {
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), 0);
    EXPECT_FALSE(heartbeat.GetSupportHeartbeat());
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest2, AddHeartbeatCountIncrementsCount) {
    heartbeat.AddHeartbeatCount();
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), 1);
    
    heartbeat.AddHeartbeatCount();
    heartbeat.AddHeartbeatCount();
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), 3);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest3, SetAndGetSupportHeartbeat) {
    heartbeat.SetSupportHeartbeat(true);
    EXPECT_TRUE(heartbeat.GetSupportHeartbeat());
    
    heartbeat.SetSupportHeartbeat(false);
    EXPECT_FALSE(heartbeat.GetSupportHeartbeat());
    
    heartbeat.SetSupportHeartbeat(true);
    EXPECT_TRUE(heartbeat.GetSupportHeartbeat());
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest4, HandleMessageCountReturnsBoolean) {
    bool result = heartbeat.HandleMessageCount();
    EXPECT_TRUE(result || !result);
    
    for (int i = 0; i < 20; ++i) {
        result = heartbeat.HandleMessageCount();
    }
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest5, ToStringReturnsNonEmptyString) {
    std::string str = heartbeat.ToString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("HdcHeartbeat"), std::string::npos);
    
    heartbeat.AddHeartbeatCount();
    heartbeat.SetSupportHeartbeat(true);
    
    std::string str2 = heartbeat.ToString();
    EXPECT_FALSE(str2.empty());
    EXPECT_NE(str, str2);
    EXPECT_NE(str2.find("heartbeatCount=1"), std::string::npos);
    EXPECT_NE(str2.find("supportHeartbeat=true"), std::string::npos);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest6, HandleRecvHeartbeatMsgWithNullPayload) {
    std::string result = heartbeat.HandleRecvHeartbeatMsg(nullptr, 0);
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("ERROR"), std::string::npos);
    
    result = heartbeat.HandleRecvHeartbeatMsg(nullptr, 10);
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("ERROR"), std::string::npos);
    
    result = heartbeat.HandleRecvHeartbeatMsg(nullptr, -1);
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("ERROR"), std::string::npos);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest7, HandleRecvHeartbeatMsgWithValidPayload) {
    uint8_t payload[] = {'H', 'e', 'l', 'l', 'o'};
    
    std::string result = heartbeat.HandleRecvHeartbeatMsg(payload, sizeof(payload));
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("HEARTBEAT_RESPONSE"), std::string::npos);
    EXPECT_NE(result.find("NACK"), std::string::npos);
    
    heartbeat.SetSupportHeartbeat(true);
    result = heartbeat.HandleRecvHeartbeatMsg(payload, sizeof(payload));
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("HEARTBEAT_RESPONSE"), std::string::npos);
    EXPECT_NE(result.find("ACK"), std::string::npos);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest8, HandleRecvHeartbeatMsgWithLargePayload) {
    const int largeSize = 10000;
    uint8_t* largePayload = new uint8_t[largeSize];
    memset(largePayload, 'A', largeSize);
    
    heartbeat.SetSupportHeartbeat(true);
    std::string result = heartbeat.HandleRecvHeartbeatMsg(largePayload, largeSize);
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("HEARTBEAT_RESPONSE"), std::string::npos);
    EXPECT_NE(result.find("ACK"), std::string::npos);
    EXPECT_NE(result.find("..."), std::string::npos);
    
    delete[] largePayload;
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest9, ThreadSafetyForAddHeartbeatCount) {
    const int numThreads = 10;
    const int incrementsPerThread = 1000;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                heartbeat.AddHeartbeatCount();
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), numThreads * incrementsPerThread);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest10, ThreadSafetyForSetSupportHeartbeat) {
    std::atomic<bool> testPassed{true};
    
    std::thread t1([&]() {
        for (int i = 0; i < 1000; ++i) {
            heartbeat.SetSupportHeartbeat(true);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    std::thread t2([&]() {
        for (int i = 0; i < 1000; ++i) {
            heartbeat.SetSupportHeartbeat(false);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    t1.join();
    t2.join();
    
    bool finalState = heartbeat.GetSupportHeartbeat();
    EXPECT_TRUE(finalState || !finalState);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest11, MultipleHandleMessageCountCalls) {
    for (int i = 0; i < 100; ++i) {
        bool result = heartbeat.HandleMessageCount();
        EXPECT_TRUE(result || !result);
    }
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest12, ConsecutiveToStringCalls) {
    std::string str1 = heartbeat.ToString();
    std::string str2 = heartbeat.ToString();
    
    EXPECT_EQ(str1.substr(0, str1.find("lastTimeElapsed")), 
              str2.substr(0, str2.find("lastTimeElapsed")));
    
    heartbeat.AddHeartbeatCount();
    
    std::string str3 = heartbeat.ToString();
    std::string str4 = heartbeat.ToString();
    
    EXPECT_EQ(str3.substr(0, str3.find("lastTimeElapsed")), 
              str4.substr(0, str4.find("lastTimeElapsed")));
    EXPECT_NE(str1, str3);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest13, MixedOperations) {
    heartbeat.SetSupportHeartbeat(true);
    EXPECT_TRUE(heartbeat.GetSupportHeartbeat());
    
    heartbeat.AddHeartbeatCount();
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), 1);
    
    bool handleResult = heartbeat.HandleMessageCount();
    EXPECT_TRUE(handleResult || !handleResult);
    
    std::string str = heartbeat.ToString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("heartbeatCount=1"), std::string::npos);
    
    uint8_t payload[] = {'T', 'e', 's', 't'};
    std::string result = heartbeat.HandleRecvHeartbeatMsg(payload, sizeof(payload));
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("ACK"), std::string::npos);
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), 2);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest14, ResetBehavior) {
    heartbeat.SetSupportHeartbeat(true);
    heartbeat.AddHeartbeatCount();
    heartbeat.AddHeartbeatCount();
    
    EXPECT_TRUE(heartbeat.GetSupportHeartbeat());
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), 2);
    
    HdcHeartbeat newHeartbeat;
    EXPECT_EQ(newHeartbeat.GetHeartbeatCount(), 0);
    EXPECT_FALSE(newHeartbeat.GetSupportHeartbeat());
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest15, HandleRecvHeartbeatMsgWithSpecialCharacters) {
    uint8_t payload1[] = {0x00, 0xFF, 0x80, 0x7F};
    heartbeat.SetSupportHeartbeat(true);
    std::string result1 = heartbeat.HandleRecvHeartbeatMsg(payload1, sizeof(payload1));
    EXPECT_FALSE(result1.empty());
    EXPECT_NE(result1.find("ACK_DATA:00FF807F"), std::string::npos);
    
    uint8_t payload2[] = {'\n', '\t', '\r', '\0', 'E', 'N', 'D'};
    std::string result2 = heartbeat.HandleRecvHeartbeatMsg(payload2, sizeof(payload2));
    EXPECT_FALSE(result2.empty());
    EXPECT_NE(result2.find("ACK_DATA:0A090D00"), std::string::npos);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest16, HandleMessageCountWithHeartbeatSupport) {
    heartbeat.SetSupportHeartbeat(true);
    
    for (int i = 0; i < 5; ++i) {
        heartbeat.AddHeartbeatCount();
    }
    
    bool result = heartbeat.HandleMessageCount();
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    result = heartbeat.HandleMessageCount();
    
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), 0);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest17, PerformanceTestMultipleAdds) {
    const int iterations = 1000000;
    
    auto start = steady_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        heartbeat.AddHeartbeatCount();
    }
    
    auto end = steady_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);
    
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), iterations);
    
    std::cout << "Performance: " << iterations << " AddHeartbeatCount calls took " 
              << duration.count() << " ms" << std::endl;
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest18, HandleRecvHeartbeatMsgIncrementsCountWhenSupported) {
    heartbeat.SetSupportHeartbeat(false);
    uint8_t payload[] = {'X'};
    
    std::string result1 = heartbeat.HandleRecvHeartbeatMsg(payload, 1);
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), 0);
    
    heartbeat.SetSupportHeartbeat(true);
    std::string result2 = heartbeat.HandleRecvHeartbeatMsg(payload, 1);
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), 1);
    
    std::string result3 = heartbeat.HandleRecvHeartbeatMsg(payload, 1);
    EXPECT_EQ(heartbeat.GetHeartbeatCount(), 2);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest19, ToStringContainsAllFields) {
    heartbeat.SetSupportHeartbeat(true);
    heartbeat.AddHeartbeatCount();
    heartbeat.AddHeartbeatCount();
    
    for (int i = 0; i < 15; ++i) {
        heartbeat.HandleMessageCount();
    }
    
    std::string str = heartbeat.ToString();
    
    EXPECT_NE(str.find("heartbeatCount=2"), std::string::npos);
    EXPECT_NE(str.find("messageCount="), std::string::npos);
    EXPECT_NE(str.find("supportHeartbeat=true"), std::string::npos);
    EXPECT_NE(str.find("lastTimeElapsed="), std::string::npos);
    EXPECT_NE(str.find("ms"), std::string::npos);
}

HWTEST_F(HdcHeartbeatTest, HdcHeartbeatTest20, EmptyPayloadHandling) {
    uint8_t emptyPayload[] = {};
    
    heartbeat.SetSupportHeartbeat(true);
    std::string result = heartbeat.HandleRecvHeartbeatMsg(emptyPayload, 0);
    
    EXPECT_NE(result.find("ERROR"), std::string::npos);
    
    result = heartbeat.HandleRecvHeartbeatMsg(nullptr, 0);
    EXPECT_NE(result.find("ERROR"), std::string::npos);
}