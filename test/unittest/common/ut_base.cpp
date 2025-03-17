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

#include "common.h"
#include "hdc_huks.h"
#include "hks_param.h"
#include "hks_api.h"
#include <ctime>

namespace Hdc {
    namespace Base {
        void PrintLogEx(const char *func, int line, uint8_t level, const char *msg, ...)
        {
            char buf[4096] = { 0 }; // only 4k to avoid stack overflow in 32bit or L0
            va_list args;
            va_start(args, msg);
            const int size = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, msg, args);
            va_end(args);
            if (size < 0) {
                printf("vsnprintf_s failed %d \n", size);
                return;
            }
            printf("%s\n", buf);
        }

        unsigned int GetSecureRandom() // just for ut test
        {
            int tmp;
            struct HksBlob nonceBlob = { sizeof(tmp), reinterpret_cast<uint8_t*>(&tmp) };
            HksGenerateRandom(nullptr, &nonceBlob);
            return tmp;
        }
    }
}