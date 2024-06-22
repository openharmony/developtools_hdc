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
//! header

#[allow(unused)]
use crate::utils::hdc_log::*;
use core::fmt;
use std::fmt::Debug;

/// header len
pub const HEADER_LEN: u64 = 512;
const SUM_CONSTANT: u32 = 256;

/// file type
#[repr(u8)]
#[derive(Debug, PartialEq, Clone, Copy)]
pub enum TypeFlage {
    /// 无效值
    Invalid = 0u8,
    /// 0: 普通文件
    OrdinaryFile = 48u8,
    /// 1: 硬链接
    HardLink = 49u8,
    /// 2: 软链接
    SoftLink = 50u8,
    /// 3: 字符设备
    CharacterDevice = 51u8,
    /// 4: 块设备
    BlockDevice = 52u8,
    /// 5: 文件夹
    Directory = 53u8,
    /// 6: 命名管道
    Fifo = 54u8,
    /// 7: 保留字
    Reserve = 55u8,
}

impl TryFrom<u8> for TypeFlage {
    type Error = ();
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0u8 => Ok(Self::Invalid),
            48u8 => Ok(Self::OrdinaryFile),
            49u8 => Ok(Self::HardLink),
            50u8 => Ok(Self::SoftLink),
            51u8 => Ok(Self::CharacterDevice),
            52u8 => Ok(Self::BlockDevice),
            53u8 => Ok(Self::Directory),
            54u8 => Ok(Self::Fifo),
            55u8 => Ok(Self::Reserve),
            _ => Ok(Self::Invalid),
        }
    }
}

/// entry header
pub struct Header {
    /// 存储文件路径。tar只有100位，不够的使用prefix进行拼接
    name: [u8; 100],
    /// 存储文件权限
    mode: [u8; 8],
    /// 用户ID。和tar格式保持一致。暂不使用，预留字段
    uid: [u8; 8],
    /// 组ID。和uid一样，预留
    gid: [u8; 8],
    /// 文件大小。以8进制进行存储
    /// 如果是目录，则填充11个0:00000000000+NUL
    /// 如果是文件，则取出文件的字节大小，假设文件大小为；1024byte，转换到8进制字符串为：2000，不足前面补0: 00000002000+NUL
    size: [u8; 12],
    /// 文件最后修改时间，10位时间戳的8进制字符。UTC时间。暂不使用
    mtime: [u8; 12],
    /// 完整性校验。暂不使用
    chksum: [u8; 8],
    /// 文件类型
    typeflage: [u8; 1],
    /// 链接名。暂不使用
    linkname: [u8; 100],
    /// TAR数据段标识字段。不需要填00000+NUL，否则填写：ustar+NUL，表示是TAR文件数据
    magic: [u8; 6],
    /// 表示TAR文件结构的版本号
    version: [u8; 2],
    /// 计算机用户名。暂不使用
    uname: [u8; 32],
    /// 用户组名。暂不使用
    gname: [u8; 32],
    /// 主设备号，暂不使用
    devmajor: [u8; 8],
    /// 次设备号，暂不使用
    devminor: [u8; 8],
    /// 文件路径前缀
    prefix: [u8; 155],
    pad: [u8; 12],
}

impl std::fmt::Debug for Header {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "name : {}\n", self.name())
    }
}

impl Default for Header {
    fn default() -> Self {
        Self::new()
    }
}

/// ustar
const MAGIC: [u8; 6] = [b'u', b's', b't', b'a', b'r', 0x20];
const VERSION: [u8; 2] = [0x20, 0x00];
impl Header {
    /// new header
    pub fn new() -> Self {
        Self {
            name: [0u8; 100],
            mode: [0u8; 8],
            uid: [0u8; 8],
            gid: [0u8; 8],
            size: [0u8; 12],
            mtime: [0u8; 12],
            chksum: [0u8; 8],
            typeflage: [0u8; 1],
            linkname: [0u8; 100],
            magic: MAGIC,
            version: VERSION,
            uname: [0u8; 32],
            gname: [0u8; 32],
            devmajor: [0u8; 8],
            devminor: [0u8; 8],
            prefix: [0u8; 155],
            pad: [0u8; 12],
        }
    }

    /// new header form data
    pub fn create_from_raw_data(data: &[u8; 512]) -> Self {
        Self {
            name: data[0..100].try_into().unwrap(),
            mode: data[100..108].try_into().unwrap(),
            uid: data[108..116].try_into().unwrap(),
            gid: data[116..124].try_into().unwrap(),
            size: data[124..136].try_into().unwrap(),
            mtime: data[136..148].try_into().unwrap(),
            chksum: data[148..156].try_into().unwrap(),
            typeflage: data[156..157].try_into().unwrap(),
            linkname: data[157..257].try_into().unwrap(),
            magic: data[257..263].try_into().unwrap(),
            version: data[263..265].try_into().unwrap(),
            uname: data[265..297].try_into().unwrap(),
            gname: data[297..329].try_into().unwrap(),
            devmajor: data[329..337].try_into().unwrap(),
            devminor: data[337..345].try_into().unwrap(),
            prefix: data[345..500].try_into().unwrap(),
            pad: data[500..512].try_into().unwrap(),
        }
    }

    fn convert_octal_string_to_u32(data: &[u8]) -> u32 {
        let Ok(mut str) = String::from_utf8(data.to_vec()) else {
            crate::error!("from_utf8 failed");
            return 0;
        };
        str = str.replace('\0', "");
        match u32::from_str_radix(&str, 8) {
            Ok(num) => num,
            Err(e) => {
                crate::error!("convert_octal_string_to_u32 failed, {e}");
                0
            }
        }
    }

    fn convert_u32_to_octal_string(dst: &mut [u8], len: usize, data: u32) {
        let str = format!("{:0width$o}\0", data, width = len - 1);
        let bytes = str.as_bytes();
        dst.copy_from_slice(bytes);
    }

    /// Get name
    pub fn name(&self) -> String {
        let Ok(prefix) = String::from_utf8(self.prefix.to_vec()) else {
            return String::new();
        };
        let Ok(name) = String::from_utf8(self.name.to_vec()) else {
            return String::new();
        };

        let aa = prefix + &name;
        aa.replace('\0', "")
    }

    /// Update name
    pub fn updata_name(&mut self, name: String) -> Result<(), &str> {
        let mut bytes = name.into_bytes();
        bytes.push(b'\0');
        let bytes = &bytes[..];

        if bytes.len() > HEADER_LEN as usize {
            return Err("file name is too long");
        }

        match bytes.len() {
            0..=100 => {
                self.name[..bytes.len()].copy_from_slice(bytes);
            }
            101..=254 => {
                let index = bytes.len() - 100;
                let (prefix, name) = bytes.split_at(index);
                self.prefix[..prefix.len()].copy_from_slice(prefix);
                self.name.copy_from_slice(name);
            }
            _ => {
                return Err("file name is too long");
            }
        }

        Ok(())
    }

    #[allow(unused)]
    fn mode(&self) -> u32 {
        Header::convert_octal_string_to_u32(&self.mode)
    }

    ///  Update file mode
    #[allow(unused)]
    pub fn updata_mode(&mut self) {
        Header::convert_u32_to_octal_string(&mut self.mode, 8, 365);
    }

    /// Get file size
    pub fn size(&self) -> u64 {
        Header::convert_octal_string_to_u32(&self.size) as u64
    }

    /// Update file size
    pub fn updata_size(&mut self, len: usize) {
        Header::convert_u32_to_octal_string(&mut self.size, 12, len as u32);
    }

    /// Get file type
    pub fn file_type(&self) -> TypeFlage {
        TypeFlage::try_from(self.typeflage[0]).unwrap()
    }

    /// Update file type
    pub fn updata_file_type(&mut self, file_type: TypeFlage) {
        self.typeflage[0] = file_type as u8;
    }

    /// file type is invalid
    pub fn is_invalid(&self) -> bool {
        self.file_type() == TypeFlage::Invalid
    }

    fn updata_check_sum(&mut self) {
        let mut sum: u32 = 0;
        let mut check_sum = |data: &[u8]| {
            for it in data {
                sum += *it as u32;
            }
        };
        check_sum(&self.name);
        check_sum(&self.mode);
        check_sum(&self.uid);
        check_sum(&self.gid);
        check_sum(&self.size);
        check_sum(&self.mtime);
        // check_sum(&self.chksum);
        check_sum(&self.typeflage);
        check_sum(&self.linkname);
        check_sum(&self.magic);
        check_sum(&self.version);
        check_sum(&self.uname);
        check_sum(&self.gname);
        check_sum(&self.devmajor);
        check_sum(&self.devminor);
        check_sum(&self.prefix);
        check_sum(&self.pad);
        sum += SUM_CONSTANT;
        Header::convert_u32_to_octal_string(&mut self.chksum, 8, sum);
    }

    /// Get bytes
    pub fn get_bytes(&mut self, bytes: &mut [u8; 512]) {
        self.updata_check_sum();
        let mut start = 0;
        let mut end = 0;
        let mut copy_data = |data: &[u8]| {
            start = end;
            end = start + data.len();
            bytes[start..end].copy_from_slice(data);
        };
        copy_data(&self.name);
        copy_data(&self.mode);
        copy_data(&self.uid);
        copy_data(&self.gid);
        copy_data(&self.size);
        copy_data(&self.mtime);
        copy_data(&self.chksum);
        copy_data(&self.typeflage);
        copy_data(&self.linkname);
        copy_data(&self.magic);
        copy_data(&self.version);
        copy_data(&self.uname);
        copy_data(&self.gname);
        copy_data(&self.devmajor);
        copy_data(&self.devminor);
        copy_data(&self.prefix);
        copy_data(&self.pad);
    }
}
