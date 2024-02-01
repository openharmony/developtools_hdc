/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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
#include <cstdarg>
#include "securec.h"
#include "usb_util.h"

std::string GetDevPath(const std::string &path)
{
    DIR *dir = ::opendir(path.c_str());
    if (dir == nullptr) {
        printf("%s: cannot open devpath: errno: %d\n", path.c_str(), errno);
        return "";
    }

    std::string res = Hdc::USB_FFS_BASE;
    std::string node;
    int count = 0;
    struct dirent *entry = nullptr;
    while ((entry = ::readdir(dir))) {
        if (*entry->d_name == '.') {
            continue;
        }
        node = entry->d_name;
        ++count;
    }
    if (count > 1) {
        res += "hdc";
    } else {
        res += node;
    }
    ::closedir(dir);
    return res;
}

static std::vector<uint8_t> BuildPacketHeader(uint32_t sessionId, uint8_t option, uint32_t data_size)
{
    std::vector<uint8_t> vecData;
    USBHead head;
    head.sessionId = htonl(sessionId);
    for (size_t i = 0; i < sizeof(head.flag); i++) {
        head.flag[i] = USB_PACKET_FLAG.data()[i];
    }
    head.option = option;
    head.data_size = htonl(data_size);
    vecData.insert(vecData.end(), (uint8_t *)&head, (uint8_t *)&head + sizeof(USBHead));
    return vecData;
}

const std::string StringFormat(const char * const formater, va_list &vaArgs)
{
    std::vector<char> args(MAX_SIZE_IOBUF);
    const int retSize = vsnprintf_s(args.data(), MAX_SIZE_IOBUF, MAX_SIZE_IOBUF - 1, formater, vaArgs);
    if (retSize < 0) {
        return std::string("");
    } else {
        return std::string(args.data(), retSize);
    }
}

const std::string StringFormat(const char * const formater, ...)
{
    va_list vaArgs;
    va_start(vaArgs, formater);
    std::string ret = StringFormat(formater, vaArgs);
    va_end(vaArgs);
    return ret;
}

bool RunPipeComand(const char *cmdString, char *outBuf, uint16_t sizeOutBuf, bool ignoreTailLf)
{
    FILE *pipeHandle = popen(cmdString, "r");
    if (pipeHandle == nullptr) {
        return false;
    }
    int bytesRead = 0;
    int bytesOnce = 0;
    while (!feof(pipeHandle)) {
        bytesOnce = fread(outBuf, 1, sizeOutBuf - bytesRead, pipeHandle);
        if (bytesOnce <= 0) {
            break;
        }
        bytesRead += bytesOnce;
    }
    if (bytesRead && ignoreTailLf) {
        if (outBuf[bytesRead - 1] == '\n') {
            outBuf[bytesRead - 1] = '\0';
        }
    }
    pclose(pipeHandle);
    return bytesRead;
}

bool SetDevItem(const char *key, const char *value)
{
    char outBuf[256] = "";
    std::string stringBuf = StringFormat("param set %s %s", key, value);
    RunPipeComand(stringBuf.c_str(), outBuf, sizeof(outBuf), true);
    return true;
}
