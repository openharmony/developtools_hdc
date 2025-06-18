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
#include "hdc_ssl_ut.h"
#include "securec.h"
using namespace testing::ext;
using namespace testing;
namespace Hdc {
typedef size_t rsize_t;
class MockHdcSSLBase : public HdcSSLBase {
public:
    MOCK_METHOD5(RsaPubkeyEncrypt, int(const uint32_t sessionId, const unsigned char* in, int inLen,
        unsigned char* out, const std::string& pubkey));
    MOCK_METHOD0(IsHandshakeFinish, bool());
    MOCK_METHOD0(ShowSSLInfo, void());
public:
    explicit MockHdcSSLBase(HSSLInfo hSSLInfo) : HdcSSLBase(hSSLInfo)
    {
    }

    ~MockHdcSSLBase()
    {
    }

    bool SetPskCallback() override
    {
        if (SSL_CTX_set_ex_data(sslCtx, 0, preSharedKey) != 1) {
            return false;
        }
        SSL_CTX_set_psk_client_callback(sslCtx, PskClientCallback);
        return true;
    }

    void SetSSLState() override
    {
        SSL_set_connect_state(ssl);
    }

    const SSL_METHOD *SetSSLMethod() override
    {
        return TLS_client_method();
    }
};

HdcSSLTest::HdcSSLTest() {}

HdcSSLTest::~HdcSSLTest() {}

void HdcSSLTest::SetUpTestCase() {}

void HdcSSLTest::TearDownTestCase() {}

void HdcSSLTest::SetUp() {}

void HdcSSLTest::TearDown() {}

void GenerateRSAKeyPair(std::string& publicKey, std::string& privateKey)
{
    EVP_PKEY *pkey = EVP_PKEY_new();
    BIGNUM *exponent = BN_new();
    RSA *rsa = RSA_new();
    int bits = RSA_KEY_BITS;

    BN_set_word(exponent, RSA_F4);
    RSA_generate_key_ex(rsa, bits, exponent, nullptr);
    EVP_PKEY_set1_RSA(pkey, rsa);
    BIO *bio = BIO_new(BIO_s_mem());
    ASSERT_TRUE(bio != nullptr);
    ASSERT_TRUE(PEM_write_bio_PUBKEY(bio, pkey));
    char *pubkeyStr;
    long pubkeyLen = BIO_get_mem_data(bio, &pubkeyStr);
    publicKey.assign(pubkeyStr, pubkeyLen);

    BIO_free(bio);
    bio = BIO_new(BIO_s_mem());
    ASSERT_TRUE(bio != nullptr);
    ASSERT_TRUE(PEM_write_bio_PrivateKey(bio, pkey,
        nullptr, nullptr, 0, nullptr, nullptr));
    char *privkeyStr;
    long privkeyLen = BIO_get_mem_data(bio, &privkeyStr);
    privateKey.assign(privkeyStr, privkeyLen);

    BIO_free(bio);
    EVP_PKEY_free(pkey);
    BN_free(exponent);
}

EVP_PKEY* ReadPrivateKeyFromString(const std::string& privateKeyPEM)
{
    BIO *bio = BIO_new_mem_buf(privateKeyPEM.c_str(), -1);
    if (!bio) {
        std::cerr << "Error: BIO_new_mem_buf failed" << std::endl;
        return nullptr;
    }

    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    if (!pkey) {
        std::cerr << "Error: PEM_read_bio_PrivateKey failed" << std::endl;
        BIO_free(bio);
        return nullptr;
    }

    BIO_free(bio);
    return pkey;
}

int RsaPrikeyDecrypt(const unsigned char* in, int inLen, unsigned char* out, EVP_PKEY* priKey)
{
    RSA *rsa = EVP_PKEY_get1_RSA(priKey);
    int outLen = RSA_private_decrypt(inLen, in, out, rsa, RSA_PKCS1_PADDING);
    RSA_free(rsa);
    return outLen;
}

void SSLHandShakeEmulate(HdcSSLBase *sslClient, HdcSSLBase *sslServer)
{
    vector<uint8_t> buf;
    ASSERT_EQ(sslClient->PerformHandshake(buf), RET_SUCCESS);
    ASSERT_EQ(sslServer->DoBIOWrite(buf.data(), buf.size()), buf.size()); // step 1
    ASSERT_EQ(sslServer->PerformHandshake(buf), RET_SUCCESS);
    ASSERT_EQ(sslClient->DoBIOWrite(buf.data(), buf.size()), buf.size()); // step 2
    ASSERT_EQ(sslClient->PerformHandshake(buf), RET_SUCCESS);
    ASSERT_EQ(sslServer->DoBIOWrite(buf.data(), buf.size()), buf.size()); // step 3
    ASSERT_EQ(sslServer->PerformHandshake(buf), RET_SUCCESS);
    ASSERT_EQ(sslClient->DoBIOWrite(buf.data(), buf.size()), buf.size()); // step 4
    ASSERT_EQ(sslClient->PerformHandshake(buf), RET_SSL_HANDSHAKE_FINISHED);
    ASSERT_EQ(sslClient->IsHandshakeFinish(), true);
    ASSERT_EQ(sslServer->IsHandshakeFinish(), true);
}

/**
 * @tc.name: SetSSLInfoTest001
 * @tc.desc: test SetSSLInfo add info
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, SetSSLInfoTest001, TestSize.Level0)
{
    HSSLInfo hSSLInfo = new HdcSSLInfo();
    HSession hSession = new HdcSession();
    hSession->serverOrDaemon = false;
    hSession->sessionId = 123;
    HdcSSLBase::SetSSLInfo(hSSLInfo, hSession);
    ASSERT_EQ(hSSLInfo->cipher, TLS_AES_128_GCM_SHA256);
    ASSERT_EQ(hSSLInfo->isDaemon, true);
    ASSERT_EQ(hSSLInfo->sessionId, hSession->sessionId);
    delete hSSLInfo;
    delete hSession;
}

/**
 * @tc.name: InitSSLTest001
 * @tc.desc: test InitSSL as daemon role
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, InitSSLTest001, TestSize.Level0)
{
    HSSLInfo hSSLInfo = new HdcSSLInfo();
    HSession hSession = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfo, hSession);
    HdcSSLBase *sslBase = new (std::nothrow) HdcDaemonSSL(hSSLInfo);
    ASSERT_EQ(sslBase->InitSSL(), RET_SUCCESS);
    ASSERT_EQ(sslBase->isInited, true);
    ASSERT_NE(sslBase->ssl, nullptr);
    ASSERT_NE(sslBase->sslCtx, nullptr);
    ASSERT_NE(sslBase->inBIO, nullptr);
    ASSERT_NE(sslBase->outBIO, nullptr);
    delete sslBase;
    delete hSSLInfo;
    delete hSession;
}

/**
 * @tc.name: InitSSLTest002
 * @tc.desc: test InitSSL as host role
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, InitSSLTest002, TestSize.Level0)
{
    HSSLInfo hSSLInfo = new HdcSSLInfo();
    HSession hSession = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfo, hSession);
    HdcSSLBase *sslBase = new (std::nothrow) HdcHostSSL(hSSLInfo);
    ASSERT_EQ(sslBase->InitSSL(), RET_SUCCESS);
    ASSERT_EQ(sslBase->isInited, true);
    ASSERT_NE(sslBase->ssl, nullptr);
    ASSERT_NE(sslBase->sslCtx, nullptr);
    ASSERT_NE(sslBase->inBIO, nullptr);
    ASSERT_NE(sslBase->outBIO, nullptr);
    delete sslBase;
    delete hSSLInfo;
    delete hSession;
}

/**
 * @tc.name: ClearSSLTest001
 * @tc.desc: test ~HdcSSLBase as host role
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, ClearSSLTest001, TestSize.Level0)
{
    HSSLInfo hSSLInfo = new HdcSSLInfo();
    HSession hSession = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfo, hSession);
    HdcSSLBase *sslBase = new (std::nothrow) HdcHostSSL(hSSLInfo);
    ASSERT_EQ(sslBase->InitSSL(), RET_SUCCESS);
    sslBase->~HdcSSLBase();
    ASSERT_EQ(sslBase->isInited, false);
    ASSERT_EQ(sslBase->ssl, nullptr);
    ASSERT_EQ(sslBase->sslCtx, nullptr);
    ASSERT_EQ(sslBase->inBIO, nullptr);
    ASSERT_EQ(sslBase->outBIO, nullptr);
    sslBase = nullptr;
    ASSERT_EQ(sslBase, nullptr);
    delete hSSLInfo;
    delete hSession;
}

/**
 * @tc.name: ClearSSLTest002
 * @tc.desc: test ~HdcSSLBase as daemon role
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, ClearSSLTest002, TestSize.Level0)
{
    HSSLInfo hSSLInfo = new HdcSSLInfo();
    HSession hSession = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfo, hSession);
    HdcSSLBase *sslBase = new (std::nothrow) HdcDaemonSSL(hSSLInfo);
    ASSERT_EQ(sslBase->InitSSL(), RET_SUCCESS);
    sslBase->~HdcSSLBase();
    ASSERT_EQ(sslBase->isInited, false);
    ASSERT_EQ(sslBase->ssl, nullptr);
    ASSERT_EQ(sslBase->sslCtx, nullptr);
    ASSERT_EQ(sslBase->inBIO, nullptr);
    ASSERT_EQ(sslBase->outBIO, nullptr);
    sslBase = nullptr;
    ASSERT_EQ(sslBase, nullptr);
    delete hSSLInfo;
    delete hSession;
}

/**
 * @tc.name: DoSSLHandshakeTest001
 * @tc.desc: test SSLHandshake with step 1~6
 * @tc.type: FUNC
 */
// host(  ) ---(TLS handshake client hello )--> hdcd(  ) step 1
// host(  ) <--(TLS handshake server hello )--- hdcd(  ) step 2
// host(ok) ---(TLS handshake change cipher)--> hdcd(  ) step 3
// host(ok) <--(TLS handshake change cipher)--- hdcd(ok) step 4
// host(ok) ---(encrypted: CHANNEL_CLOSE   )--> hdcd(ok) step 5
// host(ok) <--(encrypted: CHANNEL_CLOSE   )--- hdcd(ok) step 6
HWTEST_F(HdcSSLTest, DoSSLHandshakeTest001, TestSize.Level0)
{
    HSSLInfo hSSLInfoDaemon = new HdcSSLInfo();
    HSSLInfo hSSLInfoHost = new HdcSSLInfo();
    HSession hSessionDaemon = new HdcSession();
    HSession hSessionHost = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfoDaemon, hSessionDaemon);
    HdcSSLBase::SetSSLInfo(hSSLInfoHost, hSessionHost);
    HdcSSLBase *sslServer = new (std::nothrow) HdcDaemonSSL(hSSLInfoDaemon);
    HdcSSLBase *sslClient = new (std::nothrow) HdcHostSSL(hSSLInfoHost);
    std::vector<unsigned char> psk(32);
    fill(psk.begin(), psk.end(), 0);
    sslClient->InputPsk(psk.data(), psk.size());
    sslServer->InputPsk(psk.data(), psk.size());
    int pskClientRet = memcmp(sslClient->preSharedKey, psk.data(), psk.size());
    int pskServerRet = memcmp(sslServer->preSharedKey, psk.data(), psk.size());
    ASSERT_EQ(pskClientRet, 0);
    ASSERT_EQ(pskServerRet, 0);
    ASSERT_EQ(sslServer->InitSSL(), RET_SUCCESS);
    ASSERT_EQ(sslClient->InitSSL(), RET_SUCCESS);
    SSLHandShakeEmulate(sslClient, sslServer);
    std::vector<uint8_t> plainTextOriginal;
    std::vector<uint8_t> plainTextAltered;
    std::string str = "hello world";
    int targetSize = HdcSSLBase::GetSSLBufLen(str.size());
    plainTextOriginal.assign(str.begin(), str.end());
    plainTextAltered.assign(str.begin(), str.end());
    int sourceSize = plainTextAltered.size();
    plainTextAltered.resize(targetSize);
    ASSERT_EQ(sslClient->Encrypt(sourceSize, plainTextAltered.data()), targetSize);
    int diffRet = memcmp(plainTextOriginal.data(), plainTextAltered.data(), plainTextAltered.size());
    ASSERT_NE(diffRet, 0);
    int index = 0;
    ASSERT_EQ(sslServer->Decrypt(targetSize, BUF_SIZE_DEFAULT16, plainTextAltered.data(), index),
        RET_SUCCESS);
    int sameRet = memcmp(plainTextOriginal.data(), plainTextAltered.data(), str.size());
    ASSERT_EQ(sameRet, 0);
    delete sslClient;
    delete sslServer;
    delete hSSLInfoHost;
    delete hSSLInfoDaemon;
    delete hSessionHost;
    delete hSessionDaemon;
}

/**
 * @tc.name: InputPskTest001
 * @tc.desc: test InputPsk function with huge size input and normal size input.
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, InputPskTest001, TestSize.Level0)
{
    HSSLInfo hSSLInfo = new HdcSSLInfo();
    HSession hSession = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfo, hSession);
    HdcSSLBase *sslClient = new (std::nothrow) HdcHostSSL(hSSLInfo);
    std::vector<unsigned char> pskHuge(BUF_SIZE_PSK * 2); // 2 times of psk size
    std::vector<unsigned char> psk(BUF_SIZE_PSK);
    fill(psk.begin(), psk.end(), 0);
    fill(pskHuge.begin(), pskHuge.end(), 0);
    ASSERT_EQ(sslClient->InputPsk(pskHuge.data(), pskHuge.size()), false);
    ASSERT_EQ(sslClient->InputPsk(psk.data(), psk.size()), true);
    for (int i = 0; i < psk.size(); ++i) {
        ASSERT_EQ(sslClient->preSharedKey[i], psk[i]);
    }
    ASSERT_EQ(sslClient->ClearPsk(), true);
    for (int i = 0; i < psk.size(); ++i) {
        ASSERT_EQ(sslClient->preSharedKey[i], 0);
    }
    delete sslClient;
    delete hSSLInfo;
    delete hSession;
}

/**
 * @tc.name: PskServerCallbackTest001
 * @tc.desc: test PskServerCallback function with normal and error input.
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, PskServerCallbackTest001, TestSize.Level0)
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    SSL *ssl;
    SSL_CTX *sslCtx;
    const SSL_METHOD *method;
    method = TLS_server_method();
    sslCtx = SSL_CTX_new(method);
    std::string pskInputStr = "01234567890123456789012345678912"; // set data
    unsigned char pskInput[BUF_SIZE_PSK];
    std::copy(pskInputStr.begin(), pskInputStr.end(), pskInput);
    ASSERT_EQ(SSL_CTX_set_ex_data(sslCtx, 0, pskInput), true);
    SSL_CTX_set_psk_server_callback(sslCtx, HdcSSLBase::PskServerCallback);
    ssl = SSL_new(sslCtx);
    SSL_set_accept_state(ssl);
    unsigned char psk[BUF_SIZE_PSK];
    char identityValid[BUF_SIZE_PSK];
    unsigned int maxPskLen = BUF_SIZE_PSK;

    unsigned int ret = HdcSSLBase::PskServerCallback(ssl, STR_PSK_IDENTITY.c_str(), psk, maxPskLen);
    ASSERT_EQ(ret, BUF_SIZE_PSK);
    for (int i = 0; i < BUF_SIZE_PSK; ++i) {
        ASSERT_EQ(psk[i], pskInput[i]);
    }
    unsigned int validLen = 0; // 无效的keyLen
    ASSERT_EQ(HdcSSLBase::PskServerCallback(ssl, STR_PSK_IDENTITY.c_str(), psk, validLen), 0);
    ASSERT_EQ(HdcSSLBase::PskServerCallback(ssl, identityValid, psk, maxPskLen), 0);
}

/**
 * @tc.name: PskClientCallbackTest001
 * @tc.desc: test PskClientCallback function with normal and error input.
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, PskClientCallbackTest001, TestSize.Level0)
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    SSL *ssl;
    SSL_CTX *sslCtx;
    const SSL_METHOD *method;
    method = TLS_client_method();
    sslCtx = SSL_CTX_new(method);
    std::string pskInputStr = "01234567890123456789012345678912";
    unsigned char pskInput[BUF_SIZE_PSK];
    std::copy(pskInputStr.begin(), pskInputStr.end(), pskInput);
    ASSERT_EQ(SSL_CTX_set_ex_data(sslCtx, 0, pskInput), true);
    SSL_CTX_set_psk_client_callback(sslCtx, HdcSSLBase::PskClientCallback);
    ssl = SSL_new(sslCtx);
    SSL_set_connect_state(ssl);
    const char* hint = STR_PSK_IDENTITY.c_str();
    char identity[BUF_SIZE_PSK];
    unsigned int maxIdentityLen = BUF_SIZE_PSK;
    unsigned char psk[BUF_SIZE_PSK];
    unsigned char pskValid[BUF_SIZE_MICRO];
    unsigned int maxPskLen = BUF_SIZE_PSK;

    unsigned int ret = HdcSSLBase::PskClientCallback(ssl, hint, identity, maxIdentityLen, psk, maxPskLen);
    ASSERT_EQ(ret, BUF_SIZE_PSK);
    for (int i = 0; i < BUF_SIZE_PSK; ++i) {
        ASSERT_EQ(psk[i], pskInput[i]);
    }
    unsigned int validLen = 0; // valid keyLen
    ASSERT_EQ(HdcSSLBase::PskClientCallback(ssl, hint, identity, maxIdentityLen, psk, validLen), 0);
    ASSERT_EQ(HdcSSLBase::PskClientCallback(ssl, hint, identity, validLen, psk, maxPskLen), 0);
    ASSERT_EQ(HdcSSLBase::PskClientCallback(ssl, hint, identity, validLen, pskValid, maxPskLen), 0);
}

/**
 * @tc.name: RsaPrikeyDecryptTest001
 * @tc.desc: test RsaPrikeyDecrypt function with normal input.
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, RsaPrikeyDecryptTest001, TestSize.Level0)
{
    HSSLInfo hSSLInfo = new HdcSSLInfo();
    HSession hSession = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfo, hSession);
    MockHdcSSLBase *sslBase = new (std::nothrow) MockHdcSSLBase(hSSLInfo);
    unsigned char in[BUF_SIZE_DEFAULT2] = "test data";
    int inLen = strlen((char*)in);
    unsigned char out[BUF_SIZE_DEFAULT2];
    int ret = sslBase->RsaPrikeyDecrypt(in, inLen, out);
    ASSERT_EQ(ret, ERR_GENERIC);
}

/**
 * @tc.name: RsaPubkeyEncryptTest001
 * @tc.desc: test RsaPubkeyEncrypt function with normal input.
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, RsaPubkeyEncryptTest001, TestSize.Level0)
{
    HSSLInfo hSSLInfo = new HdcSSLInfo();
    HSession hSession = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfo, hSession);
    MockHdcSSLBase *sslBase = new (std::nothrow) MockHdcSSLBase(hSSLInfo);
    uint32_t sessionId = 12345;
    unsigned char in[BUF_SIZE_DEFAULT2] = "test data";
    int inLen = strlen((char*)in);
    unsigned char out[BUF_SIZE_DEFAULT2];
    std::string pubkey = "public key";

    EXPECT_CALL(*sslBase, RsaPubkeyEncrypt(testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(inLen));

    int ret = sslBase->RsaPubkeyEncrypt(sessionId, in, inLen, out, pubkey);
    ASSERT_EQ(ret, inLen);
    delete sslBase;
    delete hSSLInfo;
    delete hSession;
}

/**
 * @tc.name: SetHandshakeLabelTest001
 * @tc.desc: test SetHandshakeLabel function when handshake ok.
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, SetHandshakeLabelTest001, TestSize.Level0)
{
    HSSLInfo hSSLInfoDaemon = new HdcSSLInfo();
    HSSLInfo hSSLInfoHost = new HdcSSLInfo();
    HSession hSessionDaemon = new HdcSession();
    HSession hSessionHost = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfoDaemon, hSessionDaemon);
    HdcSSLBase::SetSSLInfo(hSSLInfoHost, hSessionHost);
    HdcSSLBase *sslServer = new (std::nothrow) HdcDaemonSSL(hSSLInfoDaemon);
    HdcSSLBase *sslClient = new (std::nothrow) HdcHostSSL(hSSLInfoHost);
    std::vector<unsigned char> psk(32);
    fill(psk.begin(), psk.end(), 0);
    sslClient->InputPsk(psk.data(), psk.size());
    sslServer->InputPsk(psk.data(), psk.size());
    sslServer->InitSSL();
    sslClient->InitSSL();
    SSLHandShakeEmulate(sslClient, sslServer);

    ASSERT_EQ(sslServer->SetHandshakeLabel(hSessionDaemon), true);
    ASSERT_EQ(sslClient->SetHandshakeLabel(hSessionHost), true);
    ASSERT_EQ(hSessionDaemon->sslHandshake, true);
    ASSERT_EQ(hSessionHost->sslHandshake, true);

    delete sslClient;
    delete sslServer;
    delete hSSLInfoHost;
    delete hSSLInfoDaemon;
    delete hSessionHost;
    delete hSessionDaemon;
}

/**
 * @tc.name: SetHandshakeLabelTest002
 * @tc.desc: test SetHandshakeLabel function when handshake not ok.
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, SetHandshakeLabelTest002, TestSize.Level0)
{
    HSSLInfo hSSLInfoDaemon = new HdcSSLInfo();
    HSSLInfo hSSLInfoHost = new HdcSSLInfo();
    HSession hSessionDaemon = new HdcSession();
    HSession hSessionHost = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfoDaemon, hSessionDaemon);
    HdcSSLBase::SetSSLInfo(hSSLInfoHost, hSessionHost);
    HdcSSLBase *sslServer = new (std::nothrow) HdcDaemonSSL(hSSLInfoDaemon);
    HdcSSLBase *sslClient = new (std::nothrow) HdcHostSSL(hSSLInfoHost);
    std::vector<unsigned char> psk(32);
    fill(psk.begin(), psk.end(), 0);
    sslClient->InputPsk(psk.data(), psk.size());
    sslServer->InputPsk(psk.data(), psk.size());
    sslServer->InitSSL();
    sslClient->InitSSL();
    ASSERT_EQ(sslServer->SetHandshakeLabel(hSessionDaemon), false);
    ASSERT_EQ(sslClient->SetHandshakeLabel(hSessionHost), false);
    ASSERT_EQ(hSessionDaemon->sslHandshake, false);
    ASSERT_EQ(hSessionHost->sslHandshake, false);
    delete sslServer;
    delete sslClient;
    delete hSSLInfoDaemon;
    delete hSSLInfoHost;
    delete hSessionDaemon;
    delete hSessionHost;
}

/**
 * @tc.name: GetPskEncryptTest001
 * @tc.desc: test GetPskEncrypt function return payloadSiz
 * @tc.type: FUNC
 */
HWTEST_F(HdcSSLTest, GetPskEncryptTest001, TestSize.Level0)
{
    HSSLInfo hSSLInfo = new HdcSSLInfo();
    HSession hSession = new HdcSession();
    HdcSSLBase::SetSSLInfo(hSSLInfo, hSession);
    hSSLInfo->isDaemon = false;
    HdcSSLBase *sslBase = new (std::nothrow) HdcHostSSL(hSSLInfo);
    ASSERT_EQ(sslBase->GenPsk(), true);
    string publicKey;
    string privateKey;
    GenerateRSAKeyPair(publicKey, privateKey);
    std::unique_ptr<unsigned char[]> payload(std::make_unique<unsigned char[]>(BUF_SIZE_DEFAULT2));
    int payloadSize = sslBase->GetPskEncrypt(payload.get(), BUF_SIZE_DEFAULT2, publicKey);
    ASSERT_GT(payloadSize, 0);
    unsigned char tokenDecode[BUF_SIZE_DEFAULT] = { 0 };
    std::unique_ptr<unsigned char[]> out(std::make_unique<unsigned char[]>(BUF_SIZE_DEFAULT2));
    int tbytes = EVP_DecodeBlock(tokenDecode, payload.get(), payloadSize);
    EVP_PKEY *priKey = ReadPrivateKeyFromString(privateKey);
    int outLen = RsaPrikeyDecrypt(tokenDecode, tbytes, out.get(), priKey);
    ASSERT_EQ(outLen, BUF_SIZE_PSK);
    for (int i = 0; i < BUF_SIZE_PSK; ++i) {
        ASSERT_EQ(out.get()[i], sslBase->preSharedKey[i]);
    }
    EVP_PKEY_free(priKey);
    delete sslBase;
    delete hSession;
    delete hSSLInfo;
}
} // namespace Hdc