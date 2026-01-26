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

#ifndef HDC_SECRET_MANAGE_H
#define HDC_SECRET_MANAGE_H


#include <string>
#include <fstream>
#include <sstream>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <iomanip>
#include <unistd.h>
#include "hdc_huks.h"

const std::string HDC_RAS_KEY_FILE_PWD_KEY_ALIAS = "hdc_ras_key_file_key_alias";

class HdcSecretManage {
public:
    explicit HdcSecretManage(const std::string &pwdKeyAlias);
    int TryLoadPublicKeyInfo();
    int TryLoadPrivateKeyInfo(std::string &processMessageValue);
    uint8_t *GetPublicKeyHash();

private:
    bool LoadPrivateKeyInfo();
    void ClearPrivateKeyInfo();
    void LoadPublicKeyInfo();
    void ClearPublicKeyInfo();
    bool GetPublicKeyFingerprint();
    bool CheckPubkeyAndPrivKeyMatch();
    bool LoadPublicKeyHash();
    bool SignatureByPrivKey(const char *testData, std::vector<unsigned char> &signature, size_t &reqLen);
    bool VerifyByPublicKey(const char *testData, std::vector<unsigned char> &signature, const size_t reqLen);
    bool ReadEncryptKeyFile(std::vector<uint8_t>& fileData);
    bool DerToEvpPkey(const std::pair<uint8_t*, int>& privKeyInfo);

    EVP_PKEY *pubKey;
    EVP_PKEY *privKey;
    uint8_t publicKeyHash[SHA256_DIGEST_LENGTH] = {0};
    Hdc::HdcHuks hdcRsaHuks;
};

int HandleGetPubkeyMessage(std::string &processMessageValue);
int HandleGetSignatureMessage(std::string &processMessageValue);
#endif  // HDC_SECRET_MANAGE_H