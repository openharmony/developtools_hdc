#include "compress.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace Hdc {
bool Compress::AddPath(std::string path)
{
    if (!fs::exists(path)) {
        // WRITE_LOG(LOG_FATAL, "%s is not exist", path.c_str());
        return false;
    }

    if (fs::is_regular_file(path)) {
        AddEntry(path);
        return true;
    }

    AddEntry(path);

    for (const auto& entry : fs::directory_iterator(path)) {
        AddPath(entry.path().string());
    }
    return true;
}

void Compress::AddEntry(std::string path)
{
    auto entry(path);
    // WRITE_LOG(LOG_INFO, "AddEntry %s", path.c_str());
    /* memdump(); */
    entrys.push_back(entry);
}

bool Compress::SaveToFile(std::string localPath)
{
    if (localPath.length() <= 0) {
        localPath = "tmp.tar";
    }

    if (fs::exists(localPath) && fs::is_directory(localPath)) {
        return false;
    }

    std::ofstream file(localPath.c_str(), std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    // WRITE_LOG(LOG_INFO, "SaveToFile entrys len : %llu", entrys.size());
    for (auto& entry : entrys) {
        entry.WriteToTar(file);
    }
    file.close();
    return true;
}
}
