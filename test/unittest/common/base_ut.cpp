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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base.h"
#include "define.h"

using namespace testing::ext;
using namespace testing;

namespace Hdc {
class UpdateCmdLogSwitchTest : public ::testing::Test {
    private:
        static void SetUpTestCase(void);
        static void TearDownTestCase(void);
        void SetUp();
        void TearDown();
};

void UpdateCmdLogSwitchTest::SetUpTestCase() {}
void UpdateCmdLogSwitchTest::TearDownTestCase() {}
void UpdateCmdLogSwitchTest::SetUp() {}
void UpdateCmdLogSwitchTest::TearDown() {}
    
HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_12, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "12", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_1, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "1", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_TRUE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_0, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "0", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

HWTEST_F(UpdateCmdLogSwitchTest, UpdateCmdLogSwitch_a, TestSize.Level0) {
    setenv(ENV_SERVER_CMD_LOG.c_str(), "a", 1);
    Base::UpdateCmdLogSwitch();
    EXPECT_FALSE(Base::GetCmdLogSwitch());
}

class BaseTest : public ::testing::Test {
    private:
        static void SetUpTestCase(void);
        static void TearDownTestCase(void);
        void SetUp();
        void TearDown();
};

void BaseTest::SetUpTestCase() {}
void BaseTest::TearDownTestCase() {}
void BaseTest::SetUp() {}
void BaseTest::TearDown() {}

const Base::HdcFeatureSet& featureSet = Base::GetSupportFeature();
// 验证返回的特性集合不为空
HWTEST_F(BaseTest, FeatureSet_ReturnsNonEmpty, TestSize.Level0) {
    EXPECT_FALSE(featureSet.empty());
    EXPECT_EQ(featureSet.size(), 2);
}

// 验证多次调用返回相同引用
HWTEST_F(BaseTest, FeatureSet_ReturnsSameReferenceOnMultipleCalls, TestSize.Level0) {
    const Base::HdcFeatureSet& firstCall = Base::GetSupportFeature();
    const Base::HdcFeatureSet& secondCall = Base::GetSupportFeature();
    
    EXPECT_EQ(&firstCall, &secondCall);
    EXPECT_EQ(&featureSet, &firstCall);
}

// 验证包含预期的特性
HWTEST_F(BaseTest, FeatureSet_ContainsExpectedFeatures, TestSize.Level0) {

    EXPECT_TRUE(std::find(featureSet.begin(), featureSet.end(), FEATURE_HEARTBEAT) != featureSet.end());
    EXPECT_TRUE(std::find(featureSet.begin(), featureSet.end(), FEATURE_ENCRYPT_TCP) != featureSet.end());
}

// 验证不包含非预期的特性
HWTEST_F(BaseTest, FeatureSet_DoesNotContainUnexpectedFeatures, TestSize.Level0) {
    EXPECT_FALSE(std::find(featureSet.begin(), featureSet.end(), "unknown") != featureSet.end());
    
    EXPECT_EQ(featureSet.size(), 2);
}

// 验证当输入为空容器时，函数返回空字符串
HWTEST_F(BaseTest, FeatureToStringTest_EmptyContainer, TestSize.Level0) {
    Base::HdcFeatureSet empty_feature;
    std::string result = Base::FeatureToString(empty_feature);
    EXPECT_EQ(result, "");
}

// 验证当输入只包含一个元素时，函数返回该元素且不包含逗号
HWTEST_F(BaseTest, FeatureToStringTest_SingleElement, TestSize.Level0) {
    Base::HdcFeatureSet single_feature = {"feature1"};
    std::string result = Base::FeatureToString(single_feature);
    EXPECT_EQ(result, "feature1");
}

// 验证当输入包含两个元素时，函数正确添加逗号分隔符
HWTEST_F(BaseTest, FeatureToStringTest_TwoElements, TestSize.Level0) {
    Base::HdcFeatureSet two_features = {"feature1", "feature2"};
    std::string result = Base::FeatureToString(two_features);
    EXPECT_EQ(result, "feature1,feature2");
}

// 验证当输入包含多个元素时，函数正确处理所有元素和分隔符
HWTEST_F(BaseTest, FeatureToStringTest_MultipleElements, TestSize.Level0) {
    Base::HdcFeatureSet multiple_features = {"feature1", "feature2", "feature3", "feature4"};
    std::string result = Base::FeatureToString(multiple_features);
    EXPECT_EQ(result, "feature1,feature2,feature3,feature4");
}

// 验证当容器包含空字符串时，函数正确处理
HWTEST_F(BaseTest, FeatureToStringTest_ElementsWithEmptyString, TestSize.Level0) {
    Base::HdcFeatureSet features_with_empty = {"feature1", "", "feature3"};
    std::string result = Base::FeatureToString(features_with_empty);
    EXPECT_EQ(result, "feature1,,feature3");
}

// 验证当元素包含逗号或其他特殊字符时，函数正确处理
HWTEST_F(BaseTest, FeatureToStringTest_ElementsWithSpecialCharacters, TestSize.Level0) {
    Base::HdcFeatureSet special_features = {"feature,1", "feature:2", "feature;3"};
    std::string result = Base::FeatureToString(special_features);
    EXPECT_EQ(result, "feature,1,feature:2,feature;3");
}

// 多个特征的字符串转换
HWTEST_F(BaseTest, StringToFeatureSet_NormalCase, TestSize.Level0) {
    Base::HdcFeatureSet features;
    std::string input = "feature1,feature2,feature3";
    
    Base::StringToFeatureSet(input, features);
    
    ASSERT_EQ(features.size(), 3);
    EXPECT_EQ(features[0], "feature1");
    EXPECT_EQ(features[1], "feature2");
    EXPECT_EQ(features[2], "feature3");
}

// 测试单个特征情况
HWTEST_F(BaseTest, StringToFeatureSet_SingleFeature, TestSize.Level0) {
    Base::HdcFeatureSet features;
    std::string input = "feature1";
    
    Base::StringToFeatureSet(input, features);
    
    ASSERT_EQ(features.size(), 1);
    EXPECT_EQ(features[0], "feature1");
}

// 测试空字符串情况
HWTEST_F(BaseTest, StringToFeatureSet_EmptyString, TestSize.Level0) {
    Base::HdcFeatureSet features;
    std::string input = "";
    
    Base::StringToFeatureSet(input, features);
    
    EXPECT_TRUE(features.empty());
}

//  测试包含空元素的情况
HWTEST_F(BaseTest, StringToFeatureSet_WithEmptyElements, TestSize.Level0) {
    Base::HdcFeatureSet features;
    std::string input = "feature1,,feature2";
    
    Base::StringToFeatureSet(input, features);
    
    ASSERT_EQ(features.size(), 2);
    EXPECT_EQ(features[0], "feature1");
    EXPECT_EQ(features[1], "feature2");
}

// 测试只有分隔符的情况
HWTEST_F(BaseTest, StringToFeatureSet_OnlyDelimiter, TestSize.Level0) {
    Base::HdcFeatureSet features;
    std::string input = ",";
    
    Base::StringToFeatureSet(input, features);
    
    ASSERT_EQ(features.size(), 0);
}
} // namespace Hdc