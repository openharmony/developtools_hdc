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
#include "credential_message_test.h"
#include "credential_message.h"

using namespace testing::ext;

namespace Hdc {
void HdcCredentialMessageTest::SetUpTestCase() {}
void HdcCredentialMessageTest::TearDownTestCase() {}
void HdcCredentialMessageTest::SetUp() {}
void HdcCredentialMessageTest::TearDown() {}
bool HdcCredentialMessageTest::IsNumeric(const std::string& str)
{
    if (str.empty()) {
        return false;
    }
    for (char ch : str) {
        if (!std::isdigit(ch)) {
            return false;
        }
    }
    return true;
}

int HdcCredentialMessageTest::StripLeadingZeros(const std::string& input)
{
    if (input.empty() || input == "0") {
        return 0;
    }
    size_t firstNonZero = input.find_first_not_of('0');
    if (firstNonZero == std::string::npos) {
        return 0;
    }

    std::string numberStr = input.substr(firstNonZero);
    if (!IsNumeric(numberStr)) {
        WRITE_LOG(LOG_FATAL, "StripLeadingZeros: invalid numeric string.");
        return -1;
    }
    
    char* end = nullptr;
    long value = strtol(numberStr.c_str(), &end, 10);
    return static_cast<int>(value);
}

std::vector<uint8_t> HdcCredentialMessageTest::String2Uint8(const std::string& str, size_t len)
{
    std::vector<uint8_t> byteData(len);
    for (size_t i = 0; i < len; i++) {
        byteData[i] = static_cast<uint8_t>(str[i]);
    }
    return byteData;
}

std::string HdcCredentialMessageTest::IntToStringWithPadding(int length, int maxLen)
{
    std::string str = std::to_string(length);
    if (str.length() > static_cast<size_t>(maxLen)) {
        return "";
    }
    return std::string(static_cast<size_t>(maxLen) - str.length(), '0') + str;
}

HWTEST_F(HdcCredentialMessageTest, TestInit_EmptyStr, TestSize.Level0)
{
    // input empty string.
    CredentialMessage message("");
    EXPECT_EQ(message.GetMessageVersion(), 0);
    EXPECT_EQ(message.GetMessageMethodType(), 0);
    EXPECT_EQ(message.GetMessageBodyLen(), 0);
    EXPECT_EQ(message.GetMessageBody(), "");
}

HWTEST_F(HdcCredentialMessageTest, TestInit_ShortStr, TestSize.Level0)
{
    // input short string.
    CredentialMessage message("s");
    EXPECT_EQ(message.GetMessageVersion(), 0);
    EXPECT_EQ(message.GetMessageMethodType(), 0);
    EXPECT_EQ(message.GetMessageBodyLen(), 0);
    EXPECT_EQ(message.GetMessageBody(), "");
}

HWTEST_F(HdcCredentialMessageTest, TestInit_InvalidVersion_1, TestSize.Level0)
{
    // input invalid version.
    std::string invalidMessage = "0" + std::string(METHOD_VERSION_MAX - 1, '0');
    CredentialMessage message(invalidMessage);
    EXPECT_EQ(message.GetMessageVersion(), 0);
    EXPECT_EQ(message.GetMessageMethodType(), 0);
    EXPECT_EQ(message.GetMessageBodyLen(), 0);
    EXPECT_EQ(message.GetMessageBody(), "");
}

HWTEST_F(HdcCredentialMessageTest, TestInit_InvalidVersion_2, TestSize.Level0)
{
    // input invalid version.
    std::string invalidMessage = "a" + std::string(METHOD_VERSION_MAX - 1, '0');
    CredentialMessage message(invalidMessage);
    EXPECT_EQ(message.GetMessageVersion(), 0);
    EXPECT_EQ(message.GetMessageMethodType(), 0);
    EXPECT_EQ(message.GetMessageBodyLen(), 0);
    EXPECT_EQ(message.GetMessageBody(), "");
}

HWTEST_F(HdcCredentialMessageTest, TestInit_InvalidLength, TestSize.Level0)
{
    // input invalid length.
    std::string messageStr = "100100020";
    CredentialMessage message(messageStr);
    EXPECT_EQ(message.GetMessageVersion(), 1);
    EXPECT_EQ(message.GetMessageMethodType(), 1);
    EXPECT_EQ(message.GetMessageBodyLen(), 0);
    EXPECT_EQ(message.GetMessageBody(), "");
}

HWTEST_F(HdcCredentialMessageTest, TestInit_Valid, TestSize.Level0)
{
    std::string messageStr = "1001000200";
    CredentialMessage message(messageStr);
    EXPECT_EQ(message.GetMessageVersion(), 1);
    EXPECT_EQ(message.GetMessageMethodType(), 1);
    EXPECT_EQ(message.GetMessageBodyLen(), 2);
    EXPECT_EQ(message.GetMessageBody(), "00");
}

HWTEST_F(HdcCredentialMessageTest, TestSetMessageVersion_InvalidVersion, TestSize.Level0)
{
    // input invalid version.
    CredentialMessage message("");
    message.SetMessageVersion(0);
    EXPECT_EQ(message.GetMessageVersion(), 0);
}

HWTEST_F(HdcCredentialMessageTest, TestSetMessageVersion_ValidVersion, TestSize.Level0)
{
    // input invalid version.
    CredentialMessage message("");
    message.SetMessageVersion(3);
    EXPECT_EQ(message.GetMessageVersion(), 3);
}

HWTEST_F(HdcCredentialMessageTest, TestSetMessageBody_InvalidMessageBody, TestSize.Level0)
{
    // input invalid version.
    CredentialMessage message("");
    std::string body(MESSAGE_STR_MAX_LEN + 1, 'a');
    message.SetMessageBody(body);
    EXPECT_EQ(message.GetMessageBody(), "");
    EXPECT_EQ(message.GetMessageBodyLen(), 0);
}

HWTEST_F(HdcCredentialMessageTest, TestSetMessageBody_ValidMessageBody, TestSize.Level0)
{
    // input invalid version.
    CredentialMessage message("");
    std::string body(MESSAGE_STR_MAX_LEN / 2, 'a');
    message.SetMessageBody(body);
    EXPECT_EQ(message.GetMessageBody(), body);
    EXPECT_EQ(message.GetMessageBodyLen(), body.size());
}

HWTEST_F(HdcCredentialMessageTest, TestConstruct, TestSize.Level0)
{
    // input invalid version.
    CredentialMessage message("");
    message.SetMessageVersion(3);
    message.SetMessageMethodType(1);
    message.SetMessageBodyLen(8);
    message.SetMessageBody("12345678");
    std::string result = message.Construct();
    EXPECT_EQ(result, "3001000812345678");
}

HWTEST_F(HdcCredentialMessageTest, TestIsNumeric_EmptyStr, TestSize.Level0)
{
    // input empty string.
    EXPECT_FALSE(IsNumeric(""));
}

HWTEST_F(HdcCredentialMessageTest, TestIsNumeric_InvalidNumeric, TestSize.Level0)
{
    // input invalid numeric.
    EXPECT_FALSE(IsNumeric("12a"));
}

HWTEST_F(HdcCredentialMessageTest, TestIsNumeric, TestSize.Level0)
{
    EXPECT_TRUE(IsNumeric("123"));
}

HWTEST_F(HdcCredentialMessageTest, TestStripLeadingZeros_EmptyStr_1, TestSize.Level0)
{
    // input empty string.
    EXPECT_EQ(StripLeadingZeros(""), 0);
}

HWTEST_F(HdcCredentialMessageTest, TestStripLeadingZeros_EmptyStr_2, TestSize.Level0)
{
    // input empty string.
    EXPECT_EQ(StripLeadingZeros("0"), 0);
}

HWTEST_F(HdcCredentialMessageTest, TestStripLeadingZeros_Exclude0, TestSize.Level0)
{
    // input string with 0.
    EXPECT_EQ(StripLeadingZeros("123"), 123);
}

HWTEST_F(HdcCredentialMessageTest, TestStripLeadingZeros_IncludeLetter, TestSize.Level0)
{
    // input string with letter.
    EXPECT_EQ(StripLeadingZeros("0123a"), -1);
}

HWTEST_F(HdcCredentialMessageTest, TestStripLeadingZeros, TestSize.Level0)
{
    EXPECT_EQ(StripLeadingZeros("0123"), 123);
}

HWTEST_F(HdcCredentialMessageTest, TestString2Uint8, TestSize.Level0)
{
    std::string s = "123";
    EXPECT_EQ(String2Uint8(s, 3).size(), 3);
}

HWTEST_F(HdcCredentialMessageTest, TestIntToStringWithPadding_InvalidLength, TestSize.Level0)
{
    EXPECT_EQ(IntToStringWithPadding(123, 0), "");
}

HWTEST_F(HdcCredentialMessageTest, TestIntToStringWithPadding_ValidLength, TestSize.Level0)
{
    EXPECT_EQ(IntToStringWithPadding(123, 3), "123");
}

HWTEST_F(HdcCredentialMessageTest, TestIntToStringWithPadding_WithPadding, TestSize.Level0)
{
    EXPECT_EQ(IntToStringWithPadding(123, 4), "0123");
}

HWTEST_F(HdcCredentialMessageTest, TestSplitString, TestSize.Level0) {
    const string origString = "a,b,c";
    const string seq = ",";
    vector<string> resultStrings;
    SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 3);
    EXPECT_EQ(resultStrings[0], "a");
    EXPECT_EQ(resultStrings[1], "b");
    EXPECT_EQ(resultStrings[2], "c");
}

HWTEST_F(HdcCredentialMessageTest, TestSplitStringNullArgs, TestSize.Level0) {
    const string origString = "";
    const string seq = ",";
    vector<string> resultStrings;
    SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 0);
}

HWTEST_F(HdcCredentialMessageTest, TestSplitStringLeadingSep, TestSize.Level0) {
    const string origString = ",b,c";
    const string seq = ",";
    vector<string> resultStrings;
    SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 2);
    EXPECT_EQ(resultStrings[0], "b");
    EXPECT_EQ(resultStrings[1], "c");
}

HWTEST_F(HdcCredentialMessageTest, TestSplitStringTrailingSep, TestSize.Level0) {
    const string origString = "a,,,,,b,,,,,c";
    const string seq = ",";
    vector<string> resultStrings;
    SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 3);
    EXPECT_EQ(resultStrings[0], "a");
    EXPECT_EQ(resultStrings[1], "b");
    EXPECT_EQ(resultStrings[2], "c");
}

HWTEST_F(HdcCredentialMessageTest, TestSplitStringNoSeq, TestSize.Level0) {
    const string origString = "abcdefg";
    const string seq = ",";
    vector<string> resultStrings;
    SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 1);
    EXPECT_EQ(resultStrings[0], "abcdefg");
}

HWTEST_F(HdcCredentialMessageTest, TestSplitStringSeqEmpty, TestSize.Level0) {
    const string origString = "abcdefg";
    const string seq = "";
    vector<string> resultStrings;
    SplitString(origString, seq, resultStrings);

    EXPECT_EQ(resultStrings.size(), 0);
}
}
