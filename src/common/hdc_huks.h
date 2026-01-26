/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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
#ifndef HDC_HUKS_H
#define HDC_HUKS_H
#include <string>
#include <memory>
#include <utility>
#include "hks_type.h"
namespace Hdc {
class HdcHuks {
public:
    explicit HdcHuks(const std::string& keyAlias);
    bool AesGcmEncrypt(const uint8_t* plainData, int plainDataLen, std::vector<uint8_t>& encryptData);
    std::pair<uint8_t*, int> AesGcmDecrypt(const std::string& inputData);
    static int CaculateGcmEncryptLen(int palinDataLen);
    bool ResetHuksKey(void);
    int32_t 
private:
    bool MakeHuksParamSet(struct HksParamSet **paramSet,
        const struct HksParam *baseParams, int baseParamCount, const struct HksParam *params, int paramCount);
    bool KeyExist(struct HksParamSet *paramSet);
    struct HksParamSet* MakeAesGcmDecryptParamSets(std::vector<uint8_t>& nonce);
    struct HksParamSet* MakeAesGcmEncryptParamSets(uint8_t *nonce, int length);
    bool CheckEncryptDataLen(const std::string& encryptData);
    bool CreateAesKey(struct HksParamSet *paramSet);
    bool DeleteAesKey(HksParamSet *paramSet);
    void GenerateNonce(uint8_t *nonce, int nonceBufLen);
    std::string keyAlias;
    struct HksBlob keyBlobAlias;
};
} // namespace Hdc
#endif // HDC_HUKS_H