/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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
#ifndef HDC_SYS_PARA_H
#define HDC_SYS_PARA_H
#include <string>

namespace Hdc {
using namespace std;
bool GetDevItem(const char *key, string &out, const char *preDefine = nullptr);
bool SetDevItem(const char *key, const char *value);

}  // namespace Hdc
#endif  // HDC_BASE_H
