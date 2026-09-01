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
#ifndef HDC_FILE_DESC_WRAPPER_H
#define HDC_FILE_DESC_WRAPPER_H
#include <cstdint>

typedef void (*FileReadCallback)(int fd, const uint8_t *data, int len, void *ctx);
typedef void (*FileFinishCallback)(int fd, void *ctx);

extern "C" {
int FileDescStartRead(int fd, FileReadCallback readCb, FileFinishCallback finCb, void *ctx);
int FileDescWrite(int fd, const uint8_t *data, int len);
int FileDescWriteQueued(int fd, const uint8_t *data, int len);
void FileDescStop(int fd);
int FileDescIsBusy(int fd);
}

#endif
