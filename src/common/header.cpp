#include "header.h"
#include <string.h>
#include <sstream>
#include <string>
#include <iomanip>

namespace Hdc {
constexpr uint8_t MAGIC[HEADER_MAGIC_LEN] = {'u', 's', 't', 'a', 'r', 0x20};
constexpr uint8_t VERSION[HEADER_VERSION_LEN] = {0x20, 0x00};

std::string DecimalToOctalString(int decimalNumber, int length) {
    std::ostringstream oss;
    oss << std::oct << std::setw(length) << std::setfill('0') << decimalNumber;
    return oss.str();
}

Header::Header() {
    memset(name, 0, 100);
    memset(mode, 0, 8);
    memset(uid, 0, 8);
    memset(gid, 0, 8);
    memset(size, 0, 12);
    memset(mtime, 0, 12);
    memset(chksum, 0, 8);
    memset(typeflage, 0, 1);
    memset(linkname, 0, 100);
    memset(magic, 0, 6);
    memset(version, 0, 2);
    memset(uname, 0, 32);
    memset(gname, 0, 32);
    memset(devmajor, 0, 8);
    memset(devminor, 0, 8);
    memset(prefix, 0, 155);
    memset(pad, 0, 12);
    memcpy(magic, MAGIC, HEADER_MAGIC_LEN);
    memcpy(version, VERSION, HEADER_VERSION_LEN);
}

Header::Header(uint8_t data[512]) {
    int index = 0;
    auto func = [&data, &index](uint8_t target[], int len) {
        memcpy(target, &data[index], len);
        index += len;
    };
    /* memcpy(name, &data[index], 100); */
    /* memcpy(mode, &data[index], 8); */
    /* memcpy(uid, &data[index], 8); */
    /* memcpy(gid, &data[index], 8); */
    /* memcpy(size, &data[index], 12); */
    /* memcpy(mtime, &data[index], 12); */
    /* memcpy(chksum, &data[index], 8); */
    /* memcpy(typeflage, &data[index], 1); */
    /* memcpy(linkname, &data[index], 100); */
    /* memcpy(magic, &data[index], 6); */
    /* memcpy(version, &data[index], 2); */
    /* memcpy(uname, &data[index], 32); */
    /* memcpy(gname, &data[index], 32); */
    /* memcpy(devmajor, &data[index], 8); */
    /* memcpy(devminor, &data[index], 8); */
    /* memcpy(prefix, &data[index], 155); */
    /* memcpy(pad, &data[index], 12); */

    func(name, 100);
    func(mode, 8);
    func(uid, 8);
    func(gid, 8);
    func(size, 12);
    func(mtime, 12);
    func(chksum, 8);
    func(typeflage, 1);
    func(linkname, 100);
    func(magic, 6);
    func(version, 2);
    func(uname, 32);
    func(gname, 32);
    func(devmajor, 8);
    func(devminor, 8);
    func(prefix, 155);
    func(pad, 12);
}

std::string Header::Name() 
{
    std::string name(reinterpret_cast<char*>(prefix));
    name.append(reinterpret_cast<char*>(this->name));
    return name;
}

bool Header::UpdataName(std::string p_name)
{
    auto len = p_name.length();
    if (len >= HEADER_MAX_FILE_LEN) {
        // LOGI("len too long");
        return false;
    }
    if (len < 100) {
        snprintf(reinterpret_cast<char*>(this->name), HEADER_NAME_LEN, "%s", p_name.c_str());
    } else {
        auto prefix = p_name.substr(0, len - (HEADER_NAME_LEN - 1));
        auto name = p_name.substr(len - (HEADER_NAME_LEN - 1));
        snprintf(reinterpret_cast<char*>(this->name), HEADER_NAME_LEN, "%s", name.c_str());
        snprintf(reinterpret_cast<char*>(this->prefix), HEADER_NAME_LEN, "%s", prefix.c_str());
    }
    /* LOGI("UpdataName name:"); */
    /* memdump(this->name, HEADER_NAME_LEN); */
    /* memdump(this->prefix, HEADER_PREFIX_LEN); */

    /* uint8_t buff[512] = {0}; */
    /* GetBytes(buff); */
    return true;
}

size_t Header::Size()
{
    // LOGI("size dump:");
    memdump(this->size, HEADER_SIZE_LEN);
    std::string octalStr(reinterpret_cast<char*>(this->size));
    int num = std::stoi(octalStr, nullptr, 8);
    // LOGI("size num %d", num);
    return num;
}

void Header::UpdataSize(size_t size)
{
    auto size_str = DecimalToOctalString(size, HEADER_SIZE_LEN - 1);
    snprintf(reinterpret_cast<char*>(this->size), HEADER_SIZE_LEN, "%s", size_str.c_str());
}

TypeFlage Header::FileType()
{
    if (this->typeflage[0] < TypeFlage::OrdinaryFile || this->typeflage[0] > TypeFlage::Reserve) {
        return TypeFlage::Invalid;
    }

    return (TypeFlage)this->typeflage[0];
}

void Header::UpdataFileType(TypeFlage fileType)
{
    if (fileType < TypeFlage::OrdinaryFile || fileType > TypeFlage::Reserve) {
        this->typeflage[0] = TypeFlage::Invalid;
        return;
    }
    this->typeflage[0] = fileType;
}

bool Header::IsInvalid()
{
    return FileType() == TypeFlage::Invalid;
}

void Header::UpdataCheckSum()
{
    uint64_t sum = 0;
    auto check_sum = [&sum] (uint8_t *data, size_t len) {
        for (size_t i = 0; i < len; i++) {
            sum += data[i];
        }
    };
    check_sum(this->name, HEADER_NAME_LEN);
    check_sum(this->mode, HEADER_MODE_LEN);
    check_sum(this->uid, HEADER_UID_LEN);
    check_sum(this->gid, HEADER_GID_LEN);
    check_sum(this->size, HEADER_SIZE_LEN);
    check_sum(this->mtime, HEADER_MTIME_LEN);
    /* check_sum(chksum, HEADER_CHKSUM_LEN); */
    check_sum(typeflage, HEADER_TYPEFLAGE_LEN);
    check_sum(linkname, HEADER_LINKNAME_LEN);
    check_sum(magic, HEADER_MAGIC_LEN);
    check_sum(version, HEADER_VERSION_LEN);
    check_sum(uname, HEADER_UNAME_LEN);
    check_sum(gname, HEADER_GNAME_LEN);
    check_sum(devmajor, HEADER_DEVMAJOR_LEN);
    check_sum(devminor, HEADER_DEVMINOR_LEN);
    check_sum(prefix, HEADER_PREFIX_LEN);
    check_sum(pad, HEADER_PAD_LEN);
    sum += 256;

    auto size_str = DecimalToOctalString(sum, HEADER_CHKSUM_LEN - 1);
    snprintf(reinterpret_cast<char*>(this->chksum), HEADER_CHKSUM_LEN, "%s", size_str.c_str());
}

void Header::GetBytes(uint8_t data[512])
{
    UpdataCheckSum();
    int index = 0;
    auto func = [&index, &data](uint8_t *src, int len) {
        memcpy(&data[index], src, len);
        index += len;
    };
    func(name, HEADER_NAME_LEN);
    func(mode, HEADER_MODE_LEN);
    func(uid, HEADER_UID_LEN);
    func(gid, HEADER_GID_LEN);
    func(size, HEADER_SIZE_LEN);
    func(mtime, HEADER_MTIME_LEN);
    func(chksum, HEADER_CHKSUM_LEN);
    func(typeflage, HEADER_TYPEFLAGE_LEN);
    func(linkname, HEADER_LINKNAME_LEN);
    func(magic, HEADER_MAGIC_LEN);
    func(version, HEADER_VERSION_LEN);
    func(uname, HEADER_UNAME_LEN);
    func(gname, HEADER_GNAME_LEN);
    func(devmajor, HEADER_DEVMAJOR_LEN);
    func(devminor, HEADER_DEVMINOR_LEN);
    func(prefix, HEADER_PREFIX_LEN);
    func(pad, HEADER_PAD_LEN);
    /* LOGI("GetBytes buff:"); */
    /* memdump(data, HEADER_LEN); */
}
}
