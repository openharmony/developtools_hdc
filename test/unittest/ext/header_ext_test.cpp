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

#include <cstring>
#include <fstream>
#include <filesystem>

#include "header.h"

using namespace Hdc;

namespace {

class HeaderTest : public ::testing::Test {
protected:
    void SetUp() override { }

    void TearDown() override { }

    bool HeadersEqual(const Header& h1, const Header& h2) {
        uint8_t data1[512] = {0};
        uint8_t data2[512] = {0};

        h1.GetBytes(data1, sizeof(data1));
        h2.GetBytes(data2, sizeof(data2));

        return memcmp(data1, data2, sizeof(data1)) == 0;
    }

    void CopyStringToArray(uint8_t* dest, size_t destSize, const std::string& src) {
        size_t copySize = std::min(src.size(), destSize - 1);
        memcpy(dest, src.c_str(), copySize);
        dest[copySize] = '\0';
    }

    void CreateTestHeaderData(uint8_t* data, size_t dataSize) {
        memset(data, 0, dataSize);

        std::string name = "test_file.txt";
        memcpy(data, name.c_str(), name.size());

        std::string mode = "0644";
        memcpy(data + 100, mode.c_str(), mode.size());

        std::string size = "00000002000";
        memcpy(data + 124, size.c_str(), size.size());

        data[156] = ORDINARYFILE;

        memcpy(data + 257, "ustar", 5);

        data[263] = '0';
        data[264] = '0';
    }
};

HWTEST_F(HeaderTest, HeaderTest1, DefaultConstructor) {
    Header header;

    EXPECT_TRUE(header.IsInvalid());
    EXPECT_EQ(header.Size(), 0);
    EXPECT_EQ(header.FileType(), INVALID);

    EXPECT_EQ(header.Name(), "");
}

HWTEST_F(HeaderTest, HeaderTest2, ConstructorWithData) {
    uint8_t testData[512] = {0};
    CreateTestHeaderData(testData, sizeof(testData));
    
    Header header(testData, sizeof(testData));

    EXPECT_FALSE(header.IsInvalid());
    EXPECT_EQ(header.Name(), "test_file.txt");
    EXPECT_EQ(header.Size(), 1024);
    EXPECT_EQ(header.FileType(), ORDINARYFILE);
}

HWTEST_F(HeaderTest, HeaderTest3, ConstructorWithInvalidDataLength) {
    uint8_t testData[256] = {0};

    Header header(testData, sizeof(testData));

    EXPECT_TRUE(header.IsInvalid());
}

HWTEST_F(HeaderTest, HeaderTest4, NameMethod) {
    Header header;

    std::string shortName = "short_name.txt";
    EXPECT_TRUE(header.UpdataName(shortName));
    EXPECT_EQ(header.Name(), shortName);

    std::string longName(200, 'a');
    longName += ".txt";
    EXPECT_TRUE(header.UpdataName(longName));
    EXPECT_EQ(header.Name(), longName);

    std::string veryLongName(300, 'b');
    EXPECT_FALSE(header.UpdataName(veryLongName));
}

HWTEST_F(HeaderTest, HeaderTest5, UpdataNameVariousCases) {
    Header header;

    EXPECT_FALSE(header.UpdataName(""));

    std::string name100(100, 'c');
    EXPECT_TRUE(header.UpdataName(name100));
    EXPECT_EQ(header.Name(), name100);

    std::string name255(255, 'd');
    EXPECT_TRUE(header.UpdataName(name255));
    EXPECT_EQ(header.Name(), name255);

    std::string pathName = "/home/user/test/data/file.txt";
    EXPECT_TRUE(header.UpdataName(pathName));
    EXPECT_EQ(header.Name(), pathName);
}

HWTEST_F(HeaderTest, HeaderTest6, SizeMethods) {
    Header header;

    std::vector<size_t> testSizes = {0, 1, 100, 1024, 10000, 1048576};  // 1KB, 1MB
    
    for (size_t size : testSizes) {
        header.UpdataSize(size);
        EXPECT_EQ(header.Size(), size);
    }

    size_t largeSize = (size_t)077777777777;  // 8^12 - 1
    header.UpdataSize(largeSize);
    EXPECT_EQ(header.Size(), largeSize);

    size_t tooLargeSize = (size_t)1000000000000;
    header.UpdataSize(tooLargeSize);
    EXPECT_NE(header.Size(), tooLargeSize);
}

HWTEST_F(HeaderTest, HeaderTest7, FileTypeMethods) {
    Header header;

    std::vector<TypeFlage> testTypes = {
        ORDINARYFILE,
        HARDLINK,
        SOFTLINK,
        CHARACTERDEVICE,
        BLOCKDEVICE,
        DIRECTORY,
        FIFO,
        RESERVE
    };
    
    for (TypeFlage type : testTypes) {
        header.UpdataFileType(type);
        EXPECT_EQ(header.FileType(), type);
    }

    header.UpdataFileType(INVALID);
    EXPECT_EQ(header.FileType(), INVALID);
}

HWTEST_F(HeaderTest, HeaderTest8, IsInvalidMethod) {
    Header header;

    EXPECT_TRUE(header.IsInvalid());

    header.UpdataName("test.txt");
    header.UpdataSize(100);
    header.UpdataFileType(ORDINARYFILE);

    uint8_t testData[512] = {0};
    CreateTestHeaderData(testData, sizeof(testData));
    
    Header validHeader(testData, sizeof(testData));
    EXPECT_FALSE(validHeader.IsInvalid());
}

HWTEST_F(HeaderTest, HeaderTest9, UpdataCheckSum) {
    Header header;

    header.UpdataName("checksum_test.txt");
    header.UpdataSize(512);
    header.UpdataFileType(ORDINARYFILE);

    header.UpdataCheckSum();

    uint8_t data[512] = {0};
    header.GetBytes(data, sizeof(data));

    bool hasNonZero = false;
    for (int i = 148; i < 156; i++) {
        if (data[i] != 0) {
            hasNonZero = true;
            break;
        }
    }

    SUCCEED();
}

HWTEST_F(HeaderTest, HeaderTest10, GetBytesMethod) {
    Header header1;
    header1.UpdataName("test_get_bytes.txt");
    header1.UpdataSize(2048);
    header1.UpdataFileType(DIRECTORY);
    header1.UpdataCheckSum();

    uint8_t data[512] = {0};
    header1.GetBytes(data, sizeof(data));

    Header header2(data, sizeof(data));

    EXPECT_TRUE(HeadersEqual(header1, header2));

    EXPECT_EQ(header2.Name(), "test_get_bytes.txt");
    EXPECT_EQ(header2.Size(), 2048);
    EXPECT_EQ(header2.FileType(), DIRECTORY);
}

HWTEST_F(HeaderTest, HeaderTest11, GetBytesInvalidLength) {
    Header header;
    uint8_t data[256] = {0};

    header.GetBytes(data, sizeof(data));

    SUCCEED();
}

HWTEST_F(HeaderTest, HeaderTest12, MagicAndVersionFields) {
    Header header;
    uint8_t data[512] = {0};
    header.GetBytes(data, sizeof(data));

    uint8_t validData[512] = {0};
    CreateTestHeaderData(validData, sizeof(validData));
    
    Header validHeader(validData, sizeof(validData));
    uint8_t validBytes[512] = {0};
    validHeader.GetBytes(validBytes, sizeof(validBytes));

    EXPECT_EQ(memcmp(validBytes + 257, "ustar", 5), 0);
    EXPECT_EQ(validBytes[262], '\0');

    EXPECT_EQ(validBytes[263], '0');
    EXPECT_EQ(validBytes[264], '0');
}

HWTEST_F(HeaderTest, HeaderTest13, SerializeDeserializeAllTypes) {
    std::vector<TypeFlage> allTypes = {
        ORDINARYFILE,
        HARDLINK,
        SOFTLINK,
        CHARACTERDEVICE,
        BLOCKDEVICE,
        DIRECTORY,
        FIFO,
        RESERVE
    };
    
    for (TypeFlage type : allTypes) {
        Header header1;
        std::string fileName = "test_type_" + std::to_string(type) + ".dat";
        header1.UpdataName(fileName);
        header1.UpdataSize(1024);
        header1.UpdataFileType(type);

        uint8_t data[512] = {0};
        header1.GetBytes(data, sizeof(data));

        Header header2(data, sizeof(data));

        EXPECT_EQ(header2.Name(), fileName);
        EXPECT_EQ(header2.Size(), 1024);
        EXPECT_EQ(header2.FileType(), type);
    }
}

HWTEST_F(HeaderTest, HeaderTest14, MaxFileNameLength) {
    Header header;

    std::string maxName;
    for (int i = 0; i < HEADER_MAX_FILE_LEN; i++) {
        maxName += 'a' + (i % 26);
    }
    
    EXPECT_TRUE(header.UpdataName(maxName));
    EXPECT_EQ(header.Name(), maxName);

    std::string tooLongName = maxName + "x";
    EXPECT_FALSE(header.UpdataName(tooLongName));
}

HWTEST_F(HeaderTest, HeaderTest15, DirectorySpecialHandling) {
    Header header;

    header.UpdataFileType(DIRECTORY);
    header.UpdataName("test_directory");
    header.UpdataSize(0);
    
    EXPECT_EQ(header.FileType(), DIRECTORY);
    EXPECT_EQ(header.Size(), 0);

    uint8_t data[512] = {0};
    header.GetBytes(data, sizeof(data));

    std::string sizeStr(reinterpret_cast<char*>(data + 124), 12);
    EXPECT_EQ(sizeStr.substr(0, 11), "00000000000");
}

HWTEST_F(HeaderTest, HeaderTest16, PerformanceMultipleUpdates) {
    Header header;
    
    const int iterations = 1000;
    
    for (int i = 0; i < iterations; i++) {
        std::string fileName = "file_" + std::to_string(i) + ".txt";
        header.UpdataName(fileName);
        header.UpdataSize(i * 100);
        header.UpdataFileType(ORDINARYFILE);
        
        uint8_t data[512] = {0};
        header.GetBytes(data, sizeof(data));

        EXPECT_EQ(header.Name(), fileName);
        EXPECT_EQ(header.Size(), i * 100);
    }
}
}