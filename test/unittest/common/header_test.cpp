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
#include "header_test.h"
#include "header.h"

using namespace testing::ext;

namespace Hdc {
void HdcHeaderTest::SetUpTestCase() {}
void HdcHeaderTest::TearDownTestCase() {}
void HdcHeaderTest::SetUp() {}
void HdcHeaderTest::TearDown() {}
std::string HdcHeaderTest::DecimalToOctalString(size_t decimalNumber, int length)
{
    std::ostringstream oss;
    oss << std::oct << std::setw(length) << std::setfill('0') << decimalNumber;
    return oss.str();
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_NormalCase, TestSize.Level0)
{
    EXPECT_EQ(DecimalToOctalString(10, 4), "0012");
    EXPECT_EQ(DecimalToOctalString(15, 3), "017");
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_ZeroInput, TestSize.Level0)
{
    EXPECT_EQ(DecimalToOctalString(0, 4), "0000");
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_MaxInput, TestSize.Level0)
{
    size_t maxValue = std::numeric_limits<size_t>::max();
    std::string result = DecimalToOctalString(maxValue, 20);
    EXPECT_FALSE(result.empty());
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_InvalidLength, TestSize.Level0)
{
    EXPECT_EQ(DecimalToOctalString(10, 0), "12");
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_EdgeCases, TestSize.Level0)
{
    EXPECT_EQ(DecimalToOctalString(1, 1), "1");
    EXPECT_EQ(DecimalToOctalString(8, 2), "10");
}

HWTEST_F(HdcHeaderTest, TestName_EmptyStr, TestSize.Level0)
{
    Header header;
    EXPECT_EQ(header.Name(), "");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_ShortName, TestSize.Level0)
{
    Header header;
    std::string fileName = "short_name.txt";
    EXPECT_TRUE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "short_name.txt");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_LongName, TestSize.Level0)
{
    Header header;
    std::string fileName(HEADER_MAX_FILE_LEN + 1, 'a');
    EXPECT_FALSE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_EdgeCase, TestSize.Level0)
{
    Header header;
    std::string fileName(HEADER_MAX_FILE_LEN, 'a');
    EXPECT_FALSE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_EmptyName, TestSize.Level0)
{
    Header header;
    std::string fileName = "";
    EXPECT_TRUE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "");
}

HWTEST_F(HdcHeaderTest, TestUpdataSize_Zero, TestSize.Level0)
{
    Header header;
    header.UpdataSize(0);
    EXPECT_EQ(header.Size(), 0);
}

HWTEST_F(HdcHeaderTest, TestUpdataSize, TestSize.Level0)
{
    Header header;
    header.UpdataSize(10);
    EXPECT_EQ(header.Size(), 10);
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_INVALID, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::INVALID);
    EXPECT_EQ(header.FileType(), 0);
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_ORDINARYFILE, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::ORDINARYFILE);
    EXPECT_EQ(header.FileType(), '0');
}

HWTEST_F(HdcHeaderTest, TestIsInvalid_INVALID, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::INVALID);
    EXPECT_TRUE(header.IsInvalid());
}

HWTEST_F(HdcHeaderTest, TestIsInvalid_ORDINARYFILE, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::ORDINARYFILE);
    EXPECT_FALSE(header.IsInvalid());
}

HWTEST_F(HdcHeaderTest, TestUpdataCheckSum, TestSize.Level0)
{
    Header header;
    header.UpdataCheckSum();
    EXPECT_EQ(header.chksum[0], '0');
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_HARDLINK, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::HARDLINK);
    EXPECT_EQ(header.FileType(), '1');
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_SOFTLINK, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::SOFTLINK);
    EXPECT_EQ(header.FileType(), '2');
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_CHARACTERDEVICE, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::CHARACTERDEVICE);
    EXPECT_EQ(header.FileType(), '3');
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_BLOCKDEVICE, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::BLOCKDEVICE);
    EXPECT_EQ(header.FileType(), '4');
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_DIRECTORY, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::DIRECTORY);
    EXPECT_EQ(header.FileType(), '5');
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_FIFO, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::FIFO);
    EXPECT_EQ(header.FileType(), '6');
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_RESERVE, TestSize.Level0)
{
    Header header;
    header.UpdataFileType(TypeFlage::RESERVE);
    EXPECT_EQ(header.FileType(), '7');
}

HWTEST_F(HdcHeaderTest, TestUpdataSize_LargeNumber, TestSize.Level0)
{
    Header header;
    uint64_t largeSize = 1024 * 1024 * 1024;
    header.UpdataSize(largeSize);
    EXPECT_EQ(header.Size(), largeSize);
}

HWTEST_F(HdcHeaderTest, TestUpdataName_WithSpecialCharacters, TestSize.Level0)
{
    Header header;
    std::string fileName = "test_file-123.txt";
    EXPECT_TRUE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "test_file-123.txt");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_WithUTF8, TestSize.Level0)
{
    Header header;
    std::string fileName = "中文文件名.txt";
    EXPECT_TRUE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "中文文件名.txt");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_WithSpaces, TestSize.Level0)
{
    Header header;
    std::string fileName = "file with spaces.txt";
    EXPECT_TRUE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "file with spaces.txt");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_PathSeparator, TestSize.Level0)
{
    Header header;
    std::string fileName = "path/to/file.txt";
    EXPECT_TRUE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "path/to/file.txt");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_ExactMaxLength, TestSize.Level0)
{
    Header header;
    std::string fileName(HEADER_MAX_FILE_LEN, 'a');
    EXPECT_FALSE(header.UpdataName(fileName));
    EXPECT_EQ(header.Name(), "");
}

HWTEST_F(HdcHeaderTest, TestIsInvalid_AllFileTypes, TestSize.Level0)
{
    Header header1;
    Header header2;
    Header header3;
    Header header4;
    Header header5;
    Header header6;
    Header header7;

    header1.UpdataFileType(TypeFlage::HARDLINK);
    header2.UpdataFileType(TypeFlage::SOFTLINK);
    header3.UpdataFileType(TypeFlage::CHARACTERDEVICE);
    header4.UpdataFileType(TypeFlage::BLOCKDEVICE);
    header5.UpdataFileType(TypeFlage::DIRECTORY);
    header6.UpdataFileType(TypeFlage::FIFO);
    header7.UpdataFileType(TypeFlage::RESERVE);

    EXPECT_FALSE(header1.IsInvalid());
    EXPECT_FALSE(header2.IsInvalid());
    EXPECT_FALSE(header3.IsInvalid());
    EXPECT_FALSE(header4.IsInvalid());
    EXPECT_FALSE(header5.IsInvalid());
    EXPECT_FALSE(header6.IsInvalid());
    EXPECT_FALSE(header7.IsInvalid());
}

HWTEST_F(HdcHeaderTest, TestUpdataCheckSum_MultipleCalls, TestSize.Level0)
{
    Header header;
    header.UpdataCheckSum();
    uint8_t firstChecksum = header.chksum[0];

    header.UpdataCheckSum();
    uint8_t secondChecksum = header.chksum[0];

    EXPECT_EQ(firstChecksum, secondChecksum);
}

HWTEST_F(HdcHeaderTest, TestUpdataSize_MultipleUpdates, TestSize.Level0)
{
    Header header;
    header.UpdataSize(100);
    EXPECT_EQ(header.Size(), 100);

    header.UpdataSize(200);
    EXPECT_EQ(header.Size(), 200);

    header.UpdataSize(0);
    EXPECT_EQ(header.Size(), 0);
}

HWTEST_F(HdcHeaderTest, TestUpdataName_MultipleUpdates, TestSize.Level0)
{
    Header header;
    std::string name1 = "file1.txt";
    std::string name2 = "file2.txt";

    EXPECT_TRUE(header.UpdataName(name1));
    EXPECT_EQ(header.Name(), name1);

    EXPECT_TRUE(header.UpdataName(name2));
    EXPECT_EQ(header.Name(), name2);
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_MultipleUpdates, TestSize.Level0)
{
    Header header;

    header.UpdataFileType(TypeFlage::ORDINARYFILE);
    EXPECT_EQ(header.FileType(), '0');

    header.UpdataFileType(TypeFlage::DIRECTORY);
    EXPECT_EQ(header.FileType(), '5');

    header.UpdataFileType(TypeFlage::SOFTLINK);
    EXPECT_EQ(header.FileType(), '2');
}

HWTEST_F(HdcHeaderTest, TestHeader_DefaultValues, TestSize.Level0)
{
    Header header;
    EXPECT_EQ(header.Name(), "");
    EXPECT_EQ(header.Size(), 0);
    EXPECT_EQ(header.FileType(), 0);
    EXPECT_TRUE(header.IsInvalid());
}

HWTEST_F(HdcHeaderTest, TestHeader_CompleteSetup, TestSize.Level0)
{
    Header header;
    std::string fileName = "complete_test.txt";
    uint64_t fileSize = 1024;

    EXPECT_TRUE(header.UpdataName(fileName));
    header.UpdataSize(fileSize);
    header.UpdataFileType(TypeFlage::ORDINARYFILE);
    header.UpdataCheckSum();

    EXPECT_EQ(header.Name(), fileName);
    EXPECT_EQ(header.Size(), fileSize);
    EXPECT_EQ(header.FileType(), '0');
    EXPECT_FALSE(header.IsInvalid());
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_VariousInputs, TestSize.Level0)
{
    EXPECT_EQ(DecimalToOctalString(0, 1), "0");
    EXPECT_EQ(DecimalToOctalString(1, 1), "1");
    EXPECT_EQ(DecimalToOctalString(7, 1), "7");
    EXPECT_EQ(DecimalToOctalString(8, 2), "10");
    EXPECT_EQ(DecimalToOctalString(63, 2), "77");
    EXPECT_EQ(DecimalToOctalString(64, 3), "100");
    EXPECT_EQ(DecimalToOctalString(511, 3), "777");
    EXPECT_EQ(DecimalToOctalString(512, 4), "1000");
}

HWTEST_F(HdcHeaderTest, TestDecimalToOctalString_Padding, TestSize.Level0)
{
    EXPECT_EQ(DecimalToOctalString(1, 5), "00001");
    EXPECT_EQ(DecimalToOctalString(10, 8), "00000012");
    EXPECT_EQ(DecimalToOctalString(100, 10), "0000000144");
}

HWTEST_F(HdcHeaderTest, TestUpdataName_NullCharacters, TestSize.Level0)
{
    Header header;
    std::string fileName = "file\0name.txt";
    size_t nullPos = fileName.find('\0');
    if (nullPos != std::string::npos)
    {
        fileName = fileName.substr(0, nullPos);
    }
    EXPECT_TRUE(header.UpdataName(fileName));
}

HWTEST_F(HdcHeaderTest, TestUpdataSize_EdgeValues, TestSize.Level0)
{
    Header header;
    const uint64_t edgeValues[] = {0, 1, 255, 256, 1023, 1024, 65535, 65536,
                                   1024 * 1024 - 1, 1024 * 1024, 1024 * 1024 * 1024};
    const int numValues = sizeof(edgeValues) / sizeof(edgeValues[0]);

    for (int i = 0; i < numValues; i++)
    {
        header.UpdataSize(edgeValues[i]);
        EXPECT_EQ(header.Size(), edgeValues[i]);
    }
}

HWTEST_F(HdcHeaderTest, TestHeader_MultipleInstances, TestSize.Level0)
{
    const int numInstances = 10;
    Header headers[numInstances];

    for (int i = 0; i < numInstances; i++)
    {
        std::string fileName = "file" + std::to_string(i) + ".txt";
        EXPECT_TRUE(headers[i].UpdataName(fileName));
        headers[i].UpdataSize(i * 100);
        headers[i].UpdataFileType(TypeFlage::ORDINARYFILE);

        EXPECT_EQ(headers[i].Name(), fileName);
        EXPECT_EQ(headers[i].Size(), i * 100);
        EXPECT_EQ(headers[i].FileType(), '0');
    }
}

HWTEST_F(HdcHeaderTest, TestUpdataFileType_AllTypesSequentially, TestSize.Level0)
{
    Header header;
    const TypeFlage types[] = {
        TypeFlage::INVALID,
        TypeFlage::ORDINARYFILE,
        TypeFlage::HARDLINK,
        TypeFlage::SOFTLINK,
        TypeFlage::CHARACTERDEVICE,
        TypeFlage::BLOCKDEVICE,
        TypeFlage::DIRECTORY,
        TypeFlage::FIFO,
        TypeFlage::RESERVE};
    const int numTypes = sizeof(types) / sizeof(types[0]);

    for (int i = 0; i < numTypes; i++)
    {
        header.UpdataFileType(types[i]);

        if (types[i] == TypeFlage::INVALID)
        {
            EXPECT_EQ(header.FileType(), 0);
            EXPECT_TRUE(header.IsInvalid());
        }
        else
        {
            EXPECT_EQ(header.FileType(), static_cast<uint8_t>(types[i]));
            EXPECT_FALSE(header.IsInvalid());
        }
    }
}

HWTEST_F(HdcHeaderTest, TestHeader_StressTest, TestSize.Level0)
{
    const int iterations = 100;
    Header header;

    for (int i = 0; i < iterations; i++)
    {
        std::string fileName = "stress_test_" + std::to_string(i) + ".txt";
        uint64_t fileSize = i * 1024;

        EXPECT_TRUE(header.UpdataName(fileName));
        header.UpdataSize(fileSize);
        header.UpdataFileType(TypeFlage::ORDINARYFILE);

        EXPECT_EQ(header.Name(), fileName);
        EXPECT_EQ(header.Size(), fileSize);
        EXPECT_EQ(header.FileType(), '0');
    }
}
}
