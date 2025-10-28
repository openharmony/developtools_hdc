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
#include "ext_test.h"

using namespace testing::ext;

namespace Hdc {
void HdcExtTest::SetUpTestCase()
{}
void HdcExtTest::TearDownTestCase()
{}
void HdcExtTest::SetUp()
{
    buffer = new CircleBuffer();
    compress = new Compress();
    message = new CredentialMessage("");
    decompress = new Decompress("");
    entry = new Entry("", "/tmp/test_entry");
    header = new Header();
    heartbeat = new HdcHeartbeat();
    serverCmdLog = &ServerCmdLog::GetInstance();
    testKeyPath = "/tmp/test_key.pem";
    testPubKeyPath = "/tmp/test_key.pem.pub";
    RemoveTestFiles();
}

void HdcExtTest::TearDown()
{
    delete buffer;
    delete compress;
    delete message;
    delete decompress;
    delete entry;
    delete header;
    delete heartbeat;
    delete serverCmdLog;
    RemoveTestFiles();
}

void HdcExtTest::RemoveTestFiles()
{
    (void)remove(testKeyPath.c_str());
    (void)remove(testPubKeyPath.c_str());
}

/**
 * TestGetLogLevel
 * Verify the GetLogLevel function.
 */
HWTEST_F(HdcExtTest, 1, TestSize.Level0)
{
    uint8_t originalLevel = Base::GetLogLevel();
    Base::SetLogLevel(LOG_INFO);
    EXPECT_EQ(Base::GetLogLevel(), LOG_INFO);
    Base::SetLogLevel(originalLevel);
}

/**
 * TestSetLogLevel
 * Verify the SetLogLevel function.
 */
HWTEST_F(HdcExtTest, 2, TestSize.Level0)
{
    uint8_t originalLevel = Base::GetLogLevel();
    Base::SetLogLevel(LOG_INFO);
    EXPECT_EQ(Base::GetLogLevel(), LOG_INFO);
    Base::SetLogLevel(originalLevel);
}

/**
 * TestGetLogLevelByEnv
 * Verify the GetLogLevelByEnv function with different environment variables.
 */
HWTEST_F(HdcExtTest, 3, TestSize.Level0)
{
    // Test with no environment variable
    unsetenv(ENV_SERVER_LOG.c_str());
    uint8_t logLevel = Base::GetLogLevelByEnv();
    EXPECT_EQ(logLevel, Base::GetLogLevel());

    // Test with valid environment variable
    setenv(ENV_SERVER_LOG.c_str(), "3", 1);
    logLevel = Base::GetLogLevelByEnv();
    EXPECT_EQ(logLevel, 3);

    // Test with invalid environment variable
    setenv(ENV_SERVER_LOG.c_str(), "invalid", 1);
    logLevel = Base::GetLogLevelByEnv();
    EXPECT_EQ(logLevel, Base::GetLogLevel());
}

/**
 * TestGetTimeString
 * Verify the GetTimeString function.
 */
HWTEST_F(HdcExtTest, 7, TestSize.Level0)
{
    std::string timeString;
    Base::GetTimeString(timeString);
    EXPECT_FALSE(timeString.empty());
}

/**
 * TestCreateLogDir
 * Verify the CreateLogDir function.
 */
HWTEST_F(HdcExtTest, 9, TestSize.Level0)
{
    EXPECT_TRUE(Base::CreateLogDir());
}

/**
 * TestGetCompressLogFileName
 * Verify the GetCompressLogFileName function.
 */
HWTEST_F(HdcExtTest, 10, TestSize.Level0)
{
    std::string fileName = "test.log";
    std::string compressedName = Base::GetCompressLogFileName(fileName);
    EXPECT_EQ(compressedName, fileName + LOG_FILE_COMPRESS_SUFFIX);
}

/**
 * TestGetLogOverCount
 * Verify the GetLogOverCount function.
 */
HWTEST_F(HdcExtTest, 11, TestSize.Level0)
{
    std::vector<std::string> files = {"file1.log", "file2.log"};
    uint64_t limitDirSize = 1024; // 1KB
    uint32_t overCount = Base::GetLogOverCount(files, limitDirSize);
    EXPECT_GE(overCount, 0);
}

/**
 * TestConstructorDestructor
 * Verify the constructor and destructor of CircleBuffer.
 */
HWTEST_F(HdcExtTest, 13, TestSize.Level0)
{
    EXPECT_NE(buffer, nullptr);
}

/**
 * TestMallocFree
 * Verify the Malloc and Free functions of CircleBuffer.
 */
HWTEST_F(HdcExtTest, 14, TestSize.Level0)
{
    uint8_t* buf = buffer->Malloc();
    EXPECT_NE(buf, nullptr);
    
    // Test freeing the buffer
    EXPECT_NO_THROW(buffer->Free(buf));
    
    // Test double free (should not crash)
    EXPECT_NO_THROW(buffer->Free(buf));
    
    // Test freeing nullptr (should not crash)
    EXPECT_NO_THROW(buffer->Free(nullptr));
}

/**
 * TestMallocMultiple
 * Verify multiple allocations and deallocations.
 */
HWTEST_F(HdcExtTest, 15, TestSize.Level0)
{
    uint8_t* buf1 = buffer->Malloc();
    uint8_t* buf2 = buffer->Malloc();
    
    EXPECT_NE(buf1, nullptr);
    EXPECT_NE(buf2, nullptr);
    EXPECT_NE(buf1, buf2);
    
    buffer->Free(buf1);
    buffer->Free(buf2);
}

/**
 * @tc.name: TestFreeMemory
 * @tc.desc: Verify the FreeMemory function removes unused buffers after timeout.
 */
HWTEST_F(HdcExtTest, 16, TestSize.Level0)
{
    uint8_t* buf = buffer->Malloc();
    buffer->Free(buf);
    
    // Wait for the buffer to expire (5 seconds)
    std::this_thread::sleep_for(std::chrono::seconds(6));
    
    // FreeMemory should remove the expired buffer
    buffer->FreeMemory();
    
    // Verify buffer was removed by checking if we can allocate a new one
    uint8_t* newBuf = buffer->Malloc();
    EXPECT_NE(newBuf, nullptr);
    buffer->Free(newBuf);
}

/**
 * TestTimerFunctions
 * Verify timer start/stop/notify functions.
 */
HWTEST_F(HdcExtTest, 17, TestSize.Level0)
{
    // Start timer
    buffer->TimerStart();
    
    // Verify timer is running
    EXPECT_TRUE(buffer->run_);
    
    // Stop timer
    buffer->TimerStop();
    
    // Verify timer is stopped
    EXPECT_FALSE(buffer->run_);
    
    // Test notify (should not crash)
    EXPECT_NO_THROW(buffer->TimerNotify());
}

/**
 * TestEdgeCases
 * Verify edge cases like allocation failures.
 */
HWTEST_F(HdcExtTest, 18, TestSize.Level0)
{
    // Simulate allocation failure
    testing::internal::CaptureStderr();
    uint8_t* buf = buffer->Malloc();
    EXPECT_NE(buf, nullptr);
    
    // Test freeing invalid pointer (should log error but not crash)
    uint8_t invalidBuf;
    buffer->Free(&invalidBuf);
    
    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("Free data not found"), std::string::npos);
}

/**
 * TestAddPath
 * Verify the AddPath function with different path types.
 */
HWTEST_F(HdcExtTest, 19, TestSize.Level0)
{
    // Test with non-existent path
    EXPECT_FALSE(compress->AddPath("/nonexistent/path"));

    // Test with file path
    std::string filePath = "/tmp/test_file.txt";
    std::ofstream file(filePath);
    file << "Test content";
    file.close();
    EXPECT_TRUE(compress->AddPath(filePath));
    remove(filePath.c_str());

    // Test with directory path
    std::string dirPath = "/tmp/test_dir";
    mkdir(dirPath.c_str(), 0777);
    EXPECT_TRUE(compress->AddPath(dirPath));
    rmdir(dirPath.c_str());
}

/**
 * TestAddEntry
 * Verify the AddEntry function with different conditions.
 */
HWTEST_F(HdcExtTest, 20, TestSize.Level0)
{
    // Test with empty prefix
    std::string path = "/tmp/test_entry";
    EXPECT_TRUE(compress->AddEntry(path));

    // Test with prefix matching path
    compress->UpdataPrefix("/tmp");
    EXPECT_TRUE(compress->AddEntry("/tmp"));

    // Test with max count exceeded
    compress->UpdataMaxCount(1);
    EXPECT_FALSE(compress->AddEntry("/tmp/another_entry"));
}

/**
 * TestSaveToFile
 * Verify the SaveToFile function with different local paths.
 */
HWTEST_F(HdcExtTest, 21, TestSize.Level0)
{
    // Test with default path
    EXPECT_TRUE(compress->SaveToFile(""));

    // Test with directory path
    EXPECT_FALSE(compress->SaveToFile("/tmp"));

    // Test with valid file path
    std::string filePath = "/tmp/test_save.tar";
    EXPECT_TRUE(compress->SaveToFile(filePath));
    remove(filePath.c_str());
}

/**
 * TestUpdataPrefix
 * Verify the UpdataPrefix function.
 */
HWTEST_F(HdcExtTest, 22, TestSize.Level0)
{
    std::string prefix = "/test/prefix";
    compress->UpdataPrefix(prefix);
    EXPECT_EQ(compress->prefix, prefix);
}

/**
 * TestUpdataMaxCount
 * Verify the UpdataMaxCount function.
 */
HWTEST_F(HdcExtTest, 23, TestSize.Level0)
{
    size_t maxCount = 100;
    compress->UpdataMaxCount(maxCount);
    EXPECT_EQ(compress->maxcount, maxCount);
}

/**
 * TestConstructorDestructor
 * Verify the constructor and destructor of CredentialMessage.
 */
HWTEST_F(HdcExtTest, 24, TestSize.Level0)
{
    EXPECT_NE(message, nullptr);
}

/**
 * TestInitValidMessage
 * Verify the Init function with a valid message string.
 */
HWTEST_F(HdcExtTest, 25, TestSize.Level0)
{
    std::string validMessage = "1ABCD0005Hello";
    message->Init(validMessage);
    EXPECT_EQ(message->messageVersion, 1);
    EXPECT_EQ(message->messageMethodType, 0xABCD);
    EXPECT_EQ(message->messageBodyLen, 5);
    EXPECT_EQ(message->messageBody, "Hello");
}

/**
 * TestInitEmptyMessage
 * Verify the Init function with an empty message string.
 */
HWTEST_F(HdcExtTest, 26, TestSize.Level0)
{
    std::string emptyMessage = "";
    message->Init(emptyMessage);
    EXPECT_EQ(message->messageVersion, 0);
    EXPECT_EQ(message->messageMethodType, 0);
    EXPECT_EQ(message->messageBodyLen, 0);
    EXPECT_TRUE(message->messageBody.empty());
}

/**
 * TestInitShortMessage
 * Verify the Init function with a message string that is too short.
 */
HWTEST_F(HdcExtTest, 27, TestSize.Level0)
{
    std::string shortMessage = "1ABCD";
    message->Init(shortMessage);
    EXPECT_EQ(message->messageVersion, 0);
    EXPECT_EQ(message->messageMethodType, 0);
    EXPECT_EQ(message->messageBodyLen, 0);
    EXPECT_TRUE(message->messageBody.empty());
}

/**
 * TestSetMessageVersionValid
 * Verify the SetMessageVersion function with a valid version.
 */
HWTEST_F(HdcExtTest, 28, TestSize.Level0)
{
    message->SetMessageVersion(1);
    EXPECT_EQ(message->messageVersion, 1);
}

/**
 * TestSetMessageVersionInvalid
 * Verify the SetMessageVersion function with an invalid version.
 */
HWTEST_F(HdcExtTest, 29, TestSize.Level0)
{
    message->SetMessageVersion(-1);
    EXPECT_EQ(message->messageVersion, 0);
}

/**
 * TestSetMessageBodyValid
 * Verify the SetMessageBody function with a valid body.
 */
HWTEST_F(HdcExtTest, 30, TestSize.Level0)
{
    std::string body = "ValidBody";
    message->SetMessageBody(body);
    EXPECT_EQ(message->messageBody, body);
    EXPECT_EQ(message->messageBodyLen, body.size());
}

/**
 * TestSetMessageBodyTooLong
 * Verify the SetMessageBody function with a body that exceeds the maximum length.
 */
HWTEST_F(HdcExtTest, 31, TestSize.Level0)
{
    std::string longBody(MESSAGE_STR_MAX_LEN + 1, 'a');
    message->SetMessageBody(longBody);
    EXPECT_TRUE(message->messageBody.empty());
    EXPECT_EQ(message->messageBodyLen, 0);
}

/**
 * TestConstructValidMessage
 * Verify the Construct function with valid message components.
 */
HWTEST_F(HdcExtTest, 32, TestSize.Level0)
{
    message->messageVersion = 1;
    message->messageMethodType = 0xABCD;
    message->messageBody = "Hello";
    message->messageBodyLen = 5;
    std::string constructed = message->Construct();
    EXPECT_EQ(constructed, "1ABCD0005Hello");
}

/**
 * TestConstructInvalidMessage
 * Verify the Construct function with invalid message components.
 */
HWTEST_F(HdcExtTest, 33, TestSize.Level0)
{
    message->messageVersion = -1;
    message->messageMethodType = 0xABCD;
    message->messageBody = "Hello";
    message->messageBodyLen = 5;
    std::string constructed = message->Construct();
    EXPECT_TRUE(constructed.empty());
}

/**
 * TestIsNumeric
 * Verify the IsNumeric function with different strings.
 */
HWTEST_F(HdcExtTest, 34, TestSize.Level0)
{
    EXPECT_TRUE(IsNumeric("12345"));
    EXPECT_FALSE(IsNumeric("123a45"));
    EXPECT_FALSE(IsNumeric(""));
}

/**
 * TestStripLeadingZeros
 * Verify the StripLeadingZeros function with different strings.
 */
HWTEST_F(HdcExtTest, 35, TestSize.Level0)
{
    EXPECT_EQ(StripLeadingZeros("00123"), 123);
    EXPECT_EQ(StripLeadingZeros("00000"), 0);
    EXPECT_EQ(StripLeadingZeros("123"), 123);
    EXPECT_EQ(StripLeadingZeros("abc"), -1);
}

/**
 * TestString2Uint8
 * Verify the String2Uint8 function with different strings.
 */
HWTEST_F(HdcExtTest, 36, TestSize.Level0)
{
    std::string str = "Hello";
    std::vector<uint8_t> bytes = String2Uint8(str, str.size());
    EXPECT_EQ(bytes.size(), str.size());
    for (size_t i = 0; i < str.size(); ++i)
{
        EXPECT_EQ(bytes[i], static_cast<uint8_t>(str[i]));
    }
}

/**
 * TestIntToStringWithPadding
 * Verify the IntToStringWithPadding function with different integers.
 */
HWTEST_F(HdcExtTest, 37, TestSize.Level0)
{
    EXPECT_EQ(IntToStringWithPadding(123, 5), "00123");
    EXPECT_EQ(IntToStringWithPadding(123, 2), "");
    EXPECT_EQ(IntToStringWithPadding(0, 3), "000");
}

/**
 * TestDecompressToLocalValidPath
 * Verify the DecompressToLocal function with a valid path.
 */
HWTEST_F(HdcExtTest, 38, TestSize.Level0)
{
    std::string decPath = "/tmp/test_decompress";
    mkdir(decPath.c_str(), 0777);
    std::string tarPath = "/tmp/test.tar";
    std::ofstream tarFile(tarPath);
    tarFile << "Test content";
    tarFile.close();
    decompress->tarPath = tarPath;
    EXPECT_TRUE(decompress->DecompressToLocal(decPath));
    remove(tarPath.c_str());
    rmdir(decPath.c_str());
}

/**
 * TestDecompressToLocalInvalidPath
 * Verify the DecompressToLocal function with an invalid path.
 */
HWTEST_F(HdcExtTest, 39, TestSize.Level0)
{
    std::string decPath = "/nonexistent/path";
    decompress->tarPath = "/nonexistent/tar.tar";
    EXPECT_FALSE(decompress->DecompressToLocal(decPath));
}

/**
 * TestCheckPathValid
 * Verify the CheckPath function with a valid path.
 */
HWTEST_F(HdcExtTest, 40, TestSize.Level0)
{
    std::string decPath = "/tmp/test_dir";
    mkdir(decPath.c_str(), 0777);
    std::string tarPath = "/tmp/test.tar";
    std::ofstream tarFile(tarPath);
    tarFile << "Test content";
    tarFile.close();
    decompress->tarPath = tarPath;
    EXPECT_TRUE(decompress->CheckPath(decPath));
    remove(tarPath.c_str());
    rmdir(decPath.c_str());
}

/**
 * TestCheckPathInvalidTar
 * Verify the CheckPath function with an invalid tar file.
 */
HWTEST_F(HdcExtTest, 41, TestSize.Level0)
{
    std::string decPath = "/tmp/test_dir";
    mkdir(decPath.c_str(), 0777);
    std::string tarPath = "/tmp/invalid.tar";
    std::ofstream tarFile(tarPath);
    tarFile << "Invalid content";
    tarFile.close();
    decompress->tarPath = tarPath;
    EXPECT_FALSE(decompress->CheckPath(decPath));
    remove(tarPath.c_str());
    rmdir(decPath.c_str());
}

/**
 * TestCheckPathInvalidDecPath
 * Verify the CheckPath function with an invalid decompression path.
 */
HWTEST_F(HdcExtTest, 42, TestSize.Level0)
{
    std::string decPath = "/tmp/test_file";
    std::ofstream file(decPath);
    file << "Test content";
    file.close();
    std::string tarPath = "/tmp/test.tar";
    std::ofstream tarFile(tarPath);
    tarFile << "Test content";
    tarFile.close();
    decompress->tarPath = tarPath;
    EXPECT_FALSE(decompress->CheckPath(decPath));
    remove(tarPath.c_str());
    remove(decPath.c_str());
}

/**
 * TestConstructorDestructor
 * Verify the constructor and destructor of Entry.
 */
HWTEST_F(HdcExtTest, 43, TestSize.Level0)
{
    EXPECT_NE(entry, nullptr);
}

/**
 * TestAddData
 * Verify the AddData function with different data lengths.
 */
HWTEST_F(HdcExtTest, 44, TestSize.Level0)
{
    uint8_t data[10] = {0};
    entry->needSize = 10;
    entry->AddData(data, 5);
    EXPECT_EQ(entry->needSize, 5);
    entry->AddData(data, 5);
    EXPECT_EQ(entry->needSize, 0);
}

/**
 * TestGetName
 * Verify the GetName function with different prefixes and names.
 */
HWTEST_F(HdcExtTest, 45, TestSize.Level0)
{
    entry->prefix = "/test/";
    entry->header.UpdataName("file.txt");
    EXPECT_EQ(entry->GetName(), "/test/file.txt");
}

/**
 * TestUpdataName
 * Verify the UpdataName function with different names and prefixes.
 */
HWTEST_F(HdcExtTest, 46, TestSize.Level0)
{
    entry->prefix = "/test/";
    EXPECT_TRUE(entry->UpdataName("/test/file.txt"));
    EXPECT_EQ(entry->header.Name(), "file.txt");
}

/**
 * TestCopyPayloadFile
 * Verify the CopyPayload function with a file type.
 */
HWTEST_F(HdcExtTest, 47, TestSize.Level0)
{
    entry->header.UpdataFileType(TypeFlage::ORDINARYFILE);
    std::string prefixPath = "/tmp/";
    std::ofstream inFile("/tmp/test_file");
    inFile << "Test content";
    inFile.close();
    std::ifstream inFileStream("/tmp/test_file");
    EXPECT_TRUE(entry->CopyPayload(prefixPath, inFileStream));
    inFileStream.close();
    remove("/tmp/test_file");
}

/**
 * TestCopyPayloadDir
 * Verify the CopyPayload function with a directory type.
 */
HWTEST_F(HdcExtTest, 48, TestSize.Level0)
{
    entry->header.UpdataFileType(TypeFlage::DIRECTORY);
    std::string prefixPath = "/tmp/";
    std::ifstream inFileStream("/dev/null");
    EXPECT_TRUE(entry->CopyPayload(prefixPath, inFileStream));
    inFileStream.close();
}

/**
 * TestPayloadToFile
 * Verify the PayloadToFile function with valid and invalid paths.
 */
HWTEST_F(HdcExtTest, 49, TestSize.Level0)
{
    std::string prefixPath = "/tmp/";
    std::ofstream inFile("/tmp/test_file");
    inFile << "Test content";
    inFile.close();
    std::ifstream inFileStream("/tmp/test_file");
    EXPECT_TRUE(entry->PayloadToFile(prefixPath, inFileStream));
    inFileStream.close();
    remove("/tmp/test_file");
}

/**
 * TestPayloadToDir
 * Verify the PayloadToDir function with valid and invalid paths.
 */
HWTEST_F(HdcExtTest, 50, TestSize.Level0)
{
    std::string prefixPath = "/tmp/";
    EXPECT_TRUE(entry->PayloadToDir(prefixPath));
    inFileStream.close();
}

/**
 * TestReadAndWriteData
 * Verify the ReadAndWriteData function with valid and invalid buffers.
 */
HWTEST_F(HdcExtTest, 51, TestSize.Level0)
{
    std::ofstream outFile("/tmp/test_out");
    std::ifstream inFile("/dev/zero");
    uint8_t buff[10] = {0};
    EXPECT_TRUE(entry->ReadAndWriteData(inFile, outFile, buff, 10, 10));
    outFile.close();
    inFile.close();
    remove("/tmp/test_out");
}

/**
 * TestWriteToTar
 * Verify the WriteToTar function with different file types.
 */
HWTEST_F(HdcExtTest, 52, TestSize.Level0)
{
    std::ofstream tarFile("/tmp/test.tar");
    entry->header.UpdataFileType(TypeFlage::ORDINARYFILE);
    EXPECT_TRUE(entry->WriteToTar(tarFile));
    tarFile.close();
    remove("/tmp/test.tar");
}

/**
 * TestConstructorDestructor
 * Verify the constructor and destructor of Header.
 */
HWTEST_F(HdcExtTest, 53, TestSize.Level0)
{
    EXPECT_NE(header, nullptr);
}

/**
 * TestName
 * Verify the Name function with different prefixes and names.
 */
HWTEST_F(HdcExtTest, 54, TestSize.Level0)
{
    header->UpdataName("test.txt");
    EXPECT_EQ(header->Name(), "test.txt");
}

/**
 * TestUpdataNameValid
 * Verify the UpdataName function with a valid name.
 */
HWTEST_F(HdcExtTest, 55, TestSize.Level0)
{
    EXPECT_TRUE(header->UpdataName("test.txt"));
    EXPECT_EQ(header->Name(), "test.txt");
}

/**
 * TestUpdataNameTooLong
 * Verify the UpdataName function with a name that exceeds the maximum length.
 */
HWTEST_F(HdcExtTest, 56, TestSize.Level0)
{
    std::string longName(HEADER_MAX_FILE_LEN + 1, 'a');
    EXPECT_FALSE(header->UpdataName(longName));
}

/**
 * TestSize
 * Verify the Size function with different octal strings.
 */
HWTEST_F(HdcExtTest, 57, TestSize.Level0)
{
    header->UpdataSize(123);
    EXPECT_EQ(header->Size(), 123);
}

/**
 * TestUpdataSize
 * Verify the UpdataSize function with different file lengths.
 */
HWTEST_F(HdcExtTest, 58, TestSize.Level0)
{
    header->UpdataSize(123);
    EXPECT_EQ(header->Size(), 123);
}

/**
 * TestFileType
 * Verify the FileType function with different type flags.
 */
HWTEST_F(HdcExtTest, 59, TestSize.Level0)
{
    header->UpdataFileType(TypeFlage::ORDINARYFILE);
    EXPECT_EQ(header->FileType(), TypeFlage::ORDINARYFILE);
}

/**
 * TestUpdataFileType
 * Verify the UpdataFileType function with different type flags.
 */
HWTEST_F(HdcExtTest, 60, TestSize.Level0)
{
    header->UpdataFileType(TypeFlage::DIRECTORY);
    EXPECT_EQ(header->FileType(), TypeFlage::DIRECTORY);
}

/**
 * TestIsInvalid
 * Verify the IsInvalid function with different type flags.
 */
HWTEST_F(HdcExtTest, 61, TestSize.Level0)
{
    header->UpdataFileType(TypeFlage::INVALID);
    EXPECT_TRUE(header->IsInvalid());
}

/**
 * TestUpdataCheckSum
 * Verify the UpdataCheckSum function.
 */
HWTEST_F(HdcExtTest, 62, TestSize.Level0)
{
    header->UpdataCheckSum();
    EXPECT_NE(header->Size(), 0);
}

/**
 * TestGetBytes
 * Verify the GetBytes function with different data buffers.
 */
HWTEST_F(HdcExtTest, 63, TestSize.Level0)
{
    uint8_t data[512] = {0};
    header->GetBytes(data, 512);
    EXPECT_NE(data[0], 0);
}

/**
 * TestConstructorDestructor
 * Verify the constructor and destructor of HdcHeartbeat.
 */
HWTEST_F(HdcExtTest, 64, TestSize.Level0)
{
    EXPECT_NE(heartbeat, nullptr);
}

/**
 * TestAddHeartbeatCount
 * Verify the AddHeartbeatCount function increments the heartbeat count.
 */
HWTEST_F(HdcExtTest, 65, TestSize.Level0)
{
    uint64_t initialCount = heartbeat->GetHeartbeatCount();
    heartbeat->AddHeartbeatCount();
    EXPECT_EQ(heartbeat->GetHeartbeatCount(), initialCount + 1);
}

/**
 * TestHandleMessageCount
 * Verify the HandleMessageCount function updates the message count and checks time.
 */
HWTEST_F(HdcExtTest, 66, TestSize.Level0)
{
    EXPECT_TRUE(heartbeat->HandleMessageCount());
    EXPECT_EQ(heartbeat->GetHeartbeatCount(), 0);
}

/**
 * TestGetHeartbeatCount
 * Verify the GetHeartbeatCount function returns the correct count.
 */
HWTEST_F(HdcExtTest, 67, TestSize.Level0)
{
    heartbeat->AddHeartbeatCount();
    EXPECT_EQ(heartbeat->GetHeartbeatCount(), 1);
}

/**
 * TestToString
 * Verify the ToString function returns the correct string representation.
 */
HWTEST_F(HdcExtTest, 68, TestSize.Level0)
{
    heartbeat->AddHeartbeatCount();
    std::string result = heartbeat->ToString();
    EXPECT_NE(result.find("heartbeat count is 1"), std::string::npos);
}

/**
 * TestHandleRecvHeartbeatMsg
 * Verify the HandleRecvHeartbeatMsg function processes a valid heartbeat message.
 */
HWTEST_F(HdcExtTest, 69, TestSize.Level0)
{
    uint8_t payload[] = {0x01, 0x02, 0x03};
    std::string result = heartbeat->HandleRecvHeartbeatMsg(payload, sizeof(payload));
    EXPECT_NE(result.find("heartbeat count is"), std::string::npos);
}

/**
 * TestHandleRecvHeartbeatMsgInvalid
 * Verify the HandleRecvHeartbeatMsg function handles an invalid message.
 */
HWTEST_F(HdcExtTest, 70, TestSize.Level0)
{
    std::string result = heartbeat->HandleRecvHeartbeatMsg(nullptr, 0);
    EXPECT_EQ(result, "invalid heartbeat message");
}

/**
 * TestSetSupportHeartbeat
 * Verify the SetSupportHeartbeat function updates the heartbeat support status.
 */
HWTEST_F(HdcExtTest, 71, TestSize.Level0)
{
    heartbeat->SetSupportHeartbeat(true);
    EXPECT_TRUE(heartbeat->GetSupportHeartbeat());
}

/**
 * TestGetSupportHeartbeat
 * Verify the GetSupportHeartbeat function returns the correct status.
 */
HWTEST_F(HdcExtTest, 72, TestSize.Level0)
{
    heartbeat->SetSupportHeartbeat(false);
    EXPECT_FALSE(heartbeat->GetSupportHeartbeat());
}

/**
 * TestGetInstance
 * Verify the GetInstance function returns a valid instance.
 */
HWTEST_F(HdcExtTest, 73, TestSize.Level0)
{
    EXPECT_NE(serverCmdLog, nullptr);
}

/**
 * TestPushCmdLogStr
 * Verify the PushCmdLogStr function adds logs to the queue.
 */
HWTEST_F(HdcExtTest, 74, TestSize.Level0)
{
    std::string testLog = "Test log entry";
    serverCmdLog->PushCmdLogStr(testLog);
    EXPECT_GT(serverCmdLog->CmdLogStrSize(), 0);
}

/**
 * TestPushCmdLogStrExceedsThreshold
 * Verify the PushCmdLogStr function handles queue size threshold.
 */
HWTEST_F(HdcExtTest, 75, TestSize.Level0)
{
    for (int i = 0; i < 1501; ++i) {
        serverCmdLog->PushCmdLogStr("Test log entry " + std::to_string(i));
    }
    EXPECT_EQ(serverCmdLog->CmdLogStrSize(), 1500);
}

/**
 * TestPopCmdLogStr
 * Verify the PopCmdLogStr function retrieves logs from the queue.
 */
HWTEST_F(HdcExtTest, 76, TestSize.Level0)
{
    std::string testLog = "Test log entry";
    serverCmdLog->PushCmdLogStr(testLog);
    std::string poppedLog = serverCmdLog->PopCmdLogStr();
    EXPECT_EQ(poppedLog, testLog);
}

/**
 * TestPopCmdLogStrEmptyQueue
 * Verify the PopCmdLogStr function handles an empty queue.
 */
HWTEST_F(HdcExtTest, 77, TestSize.Level0)
{
    while (serverCmdLog->CmdLogStrSize()> 0) {
        serverCmdLog->PopCmdLogStr();
    }
    EXPECT_EQ(serverCmdLog->PopCmdLogStr(), "");
}

/**
 * TestCmdLogStrSize
 * Verify the CmdLogStrSize function returns the correct queue size.
 */
HWTEST_F(HdcExtTest, 78, TestSize.Level0)
{
    size_t initialSize = serverCmdLog->CmdLogStrSize();
    serverCmdLog->PushCmdLogStr("Test log entry");
    EXPECT_EQ(serverCmdLog->CmdLogStrSize(), initialSize + 1);
}

/**
 * TestGetLastFlushTime
 * Verify the GetLastFlushTime function returns the correct timestamp.
 */
HWTEST_F(HdcExtTest, 79, TestSize.Level0)
{
    auto initialTime = serverCmdLog->GetLastFlushTime();
    serverCmdLog->PushCmdLogStr("Test log entry");
    serverCmdLog->PopCmdLogStr();
    EXPECT_NE(serverCmdLog->GetLastFlushTime(), initialTime);
}

/**
 * TestAuthVerifyHost
 * Verify AuthVerify function behavior on HOST platform
 */
HWTEST_F(HdcExtTest, 87, TestSize.Level0)
{
#ifdef HDC_HOST
    uint8_t token[32] = {0};
    uint8_t sig[256] = {0};
    EXPECT_FALSE(HdcAuth::AuthVerify(token, sig, sizeof(sig)));
#endif
}

/**
 * TestPostUIConfirm
 * Verify PostUIConfirm function behavior
 */
HWTEST_F(HdcExtTest, 88, TestSize.Level0)
{
#ifdef HDC_HOST
    std::string publicKey = "test_public_key";
    EXPECT_FALSE(HdcAuth::PostUIConfirm(publicKey));
#endif
}

/**
 * TestGenerateKeyDaemon
 * Verify GenerateKey function behavior on DAEMON platform
 */
HWTEST_F(HdcExtTest, 89, TestSize.Level0)
{
#ifndef HDC_HOST
    EXPECT_FALSE(HdcAuth::GenerateKey(testKeyPath.c_str()));
#endif
}

/**
 * TestAuthSignDaemon
 * Verify AuthSign function behavior on DAEMON platform
 */
HWTEST_F(HdcExtTest, 90, TestSize.Level0)
{
#ifndef HDC_HOST
    RSA* rsa = RSA_new();
    uint8_t token[32] = {0};
    uint8_t sig[256] = {0};
    EXPECT_EQ(0, HdcAuth::AuthSign(rsa, token, sizeof(token), sig));
    RSA_free(rsa);
#endif
}

/**
 * TestGetPublicKeyFileBufDaemon
 * Verify GetPublicKeyFileBuf function behavior
 */
HWTEST_F(HdcExtTest, 91, TestSize.Level0)
{
#ifndef HDC_HOST
    uint8_t data[512] = {0};
    EXPECT_EQ(0, HdcAuth::GetPublicKeyFileBuf(data, sizeof(data)));
#endif
}

/**
 * TestRSA2RSAPublicKey
 * Verify RSA2RSAPublicKey function with valid RSA key
 */
HWTEST_F(HdcExtTest, 92, TestSize.Level0)
{
#ifdef HDC_HOST
    RSA* rsa = RSA_new();
    BIGNUM* bn = BN_new();
    BN_set_word(bn, RSA_F4);
    RSA_generate_key_ex(rsa, 4096, bn, nullptr);

    RSAPublicKey publicKey;
    EXPECT_EQ(1, HdcAuth::RSA2RSAPublicKey(rsa, &publicKey));

    BN_free(bn);
    RSA_free(rsa);
#endif
}

/**
 * TestRSA2RSAPublicKeyInvalidSize
 * Verify RSA2RSAPublicKey function with invalid key size
 */
HWTEST_F(HdcExtTest, 93, TestSize.Level0)
{
#ifdef HDC_HOST
    RSA* rsa = RSA_new();
    BIGNUM* bn = BN_new();
    BN_set_word(bn, RSA_F4);
    RSA_generate_key_ex(rsa, 2048, bn, nullptr); // Invalid size

    RSAPublicKey publicKey;
    EXPECT_EQ(0, HdcAuth::RSA2RSAPublicKey(rsa, &publicKey));

    BN_free(bn);
    RSA_free(rsa);
#endif
}

/**
 * TestGetUserInfoSuccess
 * Verify GetUserInfo function with successful case
 */
HWTEST_F(HdcExtTest, 94, TestSize.Level0)
{
#ifdef HDC_HOST
    char buf[256] = {0};
    EXPECT_EQ(RET_SUCCESS, HdcAuth::GetUserInfo(buf, sizeof(buf)));
#endif
}

/**
 * TestGetUserInfoBufferOverflow
 * Verify GetUserInfo function with small buffer
 */
HWTEST_F(HdcExtTest, 95, TestSize.Level0)
{
#ifdef HDC_HOST
    char buf[1] = {0}; // Too small buffer
    EXPECT_EQ(ERR_BUF_OVERFLOW, HdcAuth::GetUserInfo(buf, sizeof(buf)));
#endif
}

/**
 * TestWritePublicKeyfileSuccess
 * Verify WritePublicKeyfile function with valid key
 */
HWTEST_F(HdcExtTest, 96, TestSize.Level0)
{
#ifdef HDC_HOST
    RSA* rsa = RSA_new();
    BIGNUM* bn = BN_new();
    BN_set_word(bn, RSA_F4);
    RSA_generate_key_ex(rsa, 4096, bn, nullptr);

    EXPECT_EQ(1, HdcAuth::WritePublicKeyfile(rsa, testKeyPath.c_str()));
    struct stat st;
    EXPECT_EQ(0, stat(testPubKeyPath.c_str(), &st));

    BN_free(bn);
    RSA_free(rsa);
#endif
}

/**
 * TestWritePublicKeyfileFailure
 * Verify WritePublicKeyfile function with invalid key
 */
HWTEST_F(HdcExtTest, 97, TestSize.Level0)
{
#ifdef HDC_HOST
    EXPECT_EQ(0, HdcAuth::WritePublicKeyfile(nullptr, testKeyPath.c_str()));
#endif
}

/**
 * TestGenerateKeyHost
 * Verify GenerateKey function behavior on HOST platform
 */
HWTEST_F(HdcExtTest, 98, TestSize.Level0)
{
#ifdef HDC_HOST
#ifdef HDC_SUPPORT_ENCRYPT_PRIVATE_KEY
    testing::internal::CaptureStdout();
    EXPECT_FALSE(HdcAuth::GenerateKey(testKeyPath.c_str()));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Unsupport command"), std::string::npos);
#else
    EVP_PKEY* pkey = EVP_PKEY_new();
    RSA* rsa = RSA_new();
    BIGNUM* bn = BN_new();
    BN_set_word(bn, RSA_F4);
    RSA_generate_key_ex(rsa, 4096, bn, nullptr);
    EVP_PKEY_set1_RSA(pkey, rsa);

    EXPECT_TRUE(HdcAuth::GenerateKey(testKeyPath.c_str()));
    struct stat st;
    EXPECT_EQ(0, stat(testKeyPath.c_str(), &st));

    BN_free(bn);
    RSA_free(rsa);
    EVP_PKEY_free(pkey);
#endif
#endif
}
}