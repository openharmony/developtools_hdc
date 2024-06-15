#include "entry.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <optional>

namespace fs = std::filesystem;

namespace Hdc {

std::optional<std::string> strip_prefix(const std::string& str, const std::string& prefix) {
    if (str.compare(0, prefix.length(), prefix) == 0) {
        auto p_path = str.substr(prefix.length());
        /* size_t pos = p_path.find_first_not_of("/."); */
        /* if (pos != std::string::npos) { */
        /*     p_path = p_path.substr(pos); */
        /* } */
        return p_path;
    } else {
        return std::nullopt;
    }
}

Entry::Entry(std::string prefix, std::string path)
{
    // LOGI("Entry::Entry path %s, %p", path.c_str(), this);
    fs::path fsPath = path;
    fs::path prefixPath = prefix;
    this->prefix = prefixPath / "";
    if (fs::exists(fsPath)) {
        if (fs::is_directory(fsPath)) {
            header.UpdataFileType(TypeFlage::Directory);
            header.UpdataSize(0);
        } else if (fs::is_regular_file(fsPath)) {
            auto fileSize = fs::file_size(fsPath);
            header.UpdataSize(fileSize);
            need_size = fileSize;
            header.UpdataFileType(TypeFlage::OrdinaryFile);
        }
    }

    /* header.UpdataName(path); */
    UpdataName(path);
    /* LOGI("name %s", header.Name().c_str()); */
    // LOGI("name %s", GetName().c_str());
}

Entry::Entry(uint8_t data[512])
{
    header = Header(data);
    need_size = header.Size();
}


void Entry::AddData(uint8_t *data, size_t len)
{
    if (this->need_size == 0) {
        return;
    }
    if (this->need_size > len) {
        for (size_t i = 0; i < len; i++) {
            this->data.push_back(data[i]);
        }
        this->need_size -= len;
    } else {
        for (size_t i = 0; i < this->need_size; i++) {
            this->data.push_back(data[i]);
        }
        this->need_size = 0;
    }
    // LOGI("need_size = %ld", this->need_size);
}

std::string Entry::GetName()
{
    auto name = this->prefix / this->header.Name();
    return name.string();
}

bool Entry::UpdataName(std::string name)
{
    /* fs::path p_name = name; */
    /* p_name.lexically_normal(); */
    if (!this->prefix.string().empty()) {
        /* fs::path p_path = name; */
        /* if (p_path.starts_with(this->prefix)) { */
        /*     p_path = p_path.remove_prefix(this->prefix.lexicographical_relative_size()); */
        /*     return this->header.UpdataName(p_path); */
        /* } */
        auto p_path = Hdc::strip_prefix(name, this->prefix.string());
        if (p_path.has_value()) {
            return this->header.UpdataName(p_path.value());
        }
    }
    return this->header.UpdataName(name);
}

bool Entry::SaveToFile(std::string prefixPath)
{
    if (!IsFinish()) {
        return false;
    }

    switch(this->header.FileType()) {
    case TypeFlage::OrdinaryFile: {
        /* auto saveFile = prefixPath.append(this->header.Name()); */
        auto saveFile = prefixPath.append(GetName());
        std::ofstream file(saveFile, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            // LOGI("open %s fail", saveFile.c_str());
            return false;
        }
        // LOGI("saveFile %s, size %ld", saveFile.c_str(), this->data.size());
        file.write((const char*)this->data.data(), this->data.size());
        file.close();
        if (file.fail()) {
            return false;
        }
        break;
    }
    case TypeFlage::Directory: {
        /* auto dirPath = prefixPath.append(this->header.Name()); */
        auto dirPath = prefixPath.append(GetName());
        fs::create_directory(dirPath);
    }
    default:{
        return false;
    }
    }
    return true;
}

bool Entry::WriteToTar(std::ofstream &file)
{
    switch(header.FileType()) {
    case TypeFlage::OrdinaryFile: {
        char buff[HEADER_LEN] = {0};
        header.GetBytes((uint8_t*)buff);
        /* LOGI("WriteToTar buff:"); */
        /* memdump(buff, HEADER_LEN); */
        file.write(buff, HEADER_LEN);
        /* std::ifstream inFile(header.Name()); */
        std::ifstream inFile(GetName(), std::ios::binary);
        file << inFile.rdbuf();
        auto pading = HEADER_LEN - (need_size % HEADER_LEN);
        if (pading < HEADER_LEN) {
            // LOGI("pading %ld", pading);
            char buff[512] = {0};
            file.write(buff, pading);
        }
        break;
    }
    case TypeFlage::Directory: {
        char buff[HEADER_LEN] = {0};
        header.GetBytes((uint8_t*)buff);
        file.write(buff, HEADER_LEN);
        break;
    }
    default:
        return false;
    }

    return true;
}
}
