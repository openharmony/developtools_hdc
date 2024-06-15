#include "decompress.h"

#include <filesystem>
#include <sstream>
#include <fstream>
#include <optional>
#include <iostream>

namespace fs = std::filesystem;

namespace Hdc {
/* bool Decompress::ReadData() */
/* { */
/*     if (!fs::exists(tarPath) || !fs::is_regular_file(tarPath)) { */
/*         LOGI("path not exist, or path not file"); */
/*         return false; */
/*     } */

/*     auto fileSize = fs::file_size(tarPath); */
/*     if (fileSize == 0 || fileSize % HEADER_LEN != 0) { */
/*         LOGI("file is not tar"); */
/*         return false; */
/*     } */
/* } */

bool Decompress::DecompressToLocal(std::string decPath)
{
    if (!fs::exists(tarPath) || !fs::is_regular_file(tarPath)) {
        WRITE_LOG(LOG_FATAL, "%s not exist, or not file", tarPath.c_str());
        return false;
    }

    auto fileSize = fs::file_size(tarPath);
    if (fileSize == 0 || fileSize % HEADER_LEN != 0) {
        LOGI("file is not tar");
        return false;
    }

    if (fs::exists(decPath)) {
        if (fs::is_regular_file(decPath)) {
            LOGI("path is exist, and path not dir");
            return false;
        }
    } else {
        fs::create_directories(decPath);
    }

    uint8_t buff[HEADER_LEN];
    std::ifstream inFile(tarPath);

    std::optional<std::ofstream> outFile = std::nullopt;
    std::optional<Entry> entry = std::nullopt;
    while(1) {
        inFile.read(reinterpret_cast<char*>(buff), HEADER_LEN);
        auto readcnt = inFile.gcount();
        if (readcnt == 0) {
            LOGI("read EOF");
            break;
        }
        if (inFile.fail() || readcnt != HEADER_LEN) {
            LOGI("read file error");
            break;
        }
        /* LOGI("read data:"); */
        /* memdump(buff, HEADER_LEN); */
        if (!entry.has_value()) {
            LOGI("new entry =================>");
            entry = Entry(buff);
            if (entry.value().IsFinish()) {
                entry.value().SaveToFile(decPath);
                entry = std::nullopt;
            }
            continue;
        }
        entry.value().AddData(buff, HEADER_LEN);
        if (entry.value().IsFinish()) {
            entry.value().SaveToFile(decPath);
            entry = std::nullopt;
        }
        /* std::string tmp; */
        /* std::getline(std::cin, tmp); */
    }
    if (outFile.has_value()) {
        outFile.value().close();
    }
    inFile.close();

    return true;
}
}

