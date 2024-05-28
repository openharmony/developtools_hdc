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

#ifndef HDC_UTILS_H
#define HDC_UTILS_H

extern "C" {
#ifdef _WIN32
    // return value: <0 error; 0 can start new server instance; >0 server already exists
    __declspec(dllexport) int ProgramMutex(const char* procname, bool checkOrNew, const char* tmpDir);
    __declspec(dllexport) bool PullupServerWin32(const char *runPath, const char *listenString, int logLevel);
#else
    // return value: <0 error; 0 can start new server instance; >0 server already exists
    int ProgramMutex(const char* procname, bool checkOrNew, const char* tmpDir);
#endif
}

#endif
