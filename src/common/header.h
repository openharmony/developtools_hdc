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
#ifndef HDC_HEADER_H
#define HDC_HEADER_H

#include <cstdint>
#include <string>
#include "common.h"

namespace Hdc {
#define HEADER_LEN 512
#define HEADER_NAME_LEN 100
#define HEADER_MODE_LEN 8
#define HEADER_UID_LEN 8
#define HEADER_GID_LEN 8
#define HEADER_SIZE_LEN 12
#define HEADER_MTIME_LEN 12
#define HEADER_CHKSUM_LEN 8
#define HEADER_TYPEFLAGE_LEN 1
#define HEADER_LINKNAME_LEN 100
#define HEADER_MAGIC_LEN 6
#define HEADER_VERSION_LEN 2
#define HEADER_UNAME_LEN 32
#define HEADER_GNAME_LEN 32
#define HEADER_DEVMAJOR_LEN 8
#define HEADER_DEVMINOR_LEN 8
#define HEADER_PREFIX_LEN 155
#define HEADER_PAD_LEN 12
#define HEADER_MAX_FILE_LEN 255

typedef enum : uint8_t {
    /// 无效值
    INVALID = 0,
    /// 0: 普通文件
    ORDINARYFILE = '0',
    /// 1: 硬链接
    HARDLINK = '1',
    /// 2: 软链接
    SOFTLINK = '2',
    /// 3: 字符设备
    CHARACTERDEVICE = '3',
    /// 4: 块设备
    BLOCKDEVICE = '4',
    /// 5: 文件夹
    DIRECTORY = '5',
    /// 6: 命名管道
    FIFO = '6',
    /// 7: 保留字
    RESERVE = '7',
} TypeFlage;

struct Header {
    /// 存储文件路径。tar只有100位，不够的使用prefix进行拼接
    uint8_t name[100];
    /// 存储文件权限
    uint8_t mode[8];
    /// 用户ID。和tar格式保持一致。暂不使用，预留字段
    uint8_t uid[8];
    /// 组ID。和uid一样，预留
    uint8_t gid[8];
    /// 文件大小。以8进制进行存储
    /// 如果是目录，则填充11个0:00000000000+NUL
    /// 如果是文件，则取出文件的字节大小，假设文件大小为；1024byte，转换到8进制字符串为：2000，不足前面补0: 00000002000+NUL
    uint8_t size[12];
    /// 文件最后修改时间，10位时间戳的8进制字符。UTC时间。暂不使用
    uint8_t mtime[12];
    /// 完整性校验。暂不使用
    uint8_t chksum[8];
    /// 文件类型
    uint8_t typeflage[1];
    /// 链接名。暂不使用
    uint8_t linkname[100];
    /// TAR数据段标识字段。不需要填00000+NUL，否则填写：ustar+NUL，表示是TAR文件数据
    uint8_t magic[6];
    /// 表示TAR文件结构的版本号
    uint8_t version[2];
    /// 计算机用户名。暂不使用
    uint8_t uname[32];
    /// 用户组名。暂不使用
    uint8_t gname[32];
    /// 主设备号，暂不使用
    uint8_t devmajor[8];
    /// 次设备号，暂不使用
    uint8_t devminor[8];
    /// 文件路径前缀
    uint8_t prefix[155];
    uint8_t pad[12];

    Header();
    explicit Header(uint8_t data[512], int dataLen);
    std::string Name();
    bool UpdataName(std::string fileName);
    uint64_t Size();
    void UpdataSize(size_t fileLen);
    TypeFlage FileType();
    void UpdataFileType(TypeFlage fileType);
    bool IsInvalid();
    void UpdataCheckSum();
    void GetBytes(uint8_t data[512], int dataLen);
};

}
#endif
