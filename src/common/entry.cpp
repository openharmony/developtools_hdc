#include "entry.h"

#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace Hdc {
Entry::Entry(std::string path)
{
    // LOGI("Entry::Entry path %s, %p", path.c_str(), this);
    fs::path fsPath = path;
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

    header.UpdataName(path);
    // LOGI("name %s", header.Name().c_str());
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
    // WRITE_LOG(LOG_INFO, "need_size = %ld", this->need_size);
}

bool Entry::SaveToFile(std::string prefixPath)
{
    if (!IsFinish()) {
        return false;
    }

    switch(this->header.FileType()) {
    case TypeFlage::OrdinaryFile: {
        auto saveFile = prefixPath.append(this->header.Name());
        std::ofstream file(saveFile, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            // WRITE_LOG(LOG_FATAL, "open %s fail", saveFile.c_str());
            return false;
        }
        // WRITE_LOG(LOG_INFO, "saveFile %s, size %llu", saveFile.c_str(), this->data.size());
        file.write((const char*)this->data.data(), this->data.size());
        file.close();
        if (file.fail()) {
            return false;
        }
        break;
    }
    case TypeFlage::Directory: {
        auto dirPath = prefixPath.append(this->header.Name());
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
        std::ifstream inFile(header.Name());
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
