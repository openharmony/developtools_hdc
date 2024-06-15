#ifndef __COMPRESS_H_
#define __COMPRESS_H_

#include <vector>
#include <string>

#include "entry.h"

namespace Hdc {
class Compress
{
public:
    Compress() {}
    ~Compress() {}

    bool AddPath(std::string path);
    bool AddEntry(std::string path);
    bool SaveToFile(std::string localPath);
    void UpdataPrefix(std::string prefix);
    void UpdataMaxCount(size_t maxCount);
private:
    std::vector<Entry> entrys;
    std::string prefix;
    size_t max_count;
};
}

#endif
