#ifndef __COMPRESS_H_
#define __COMPRESS_H_

#include <vector>

#include "entry.h"

namespace Hdc {
class Compress
{
public:
    Compress() {}
    ~Compress() {}

    bool AddPath(std::string path);
    void AddEntry(std::string path);
    bool SaveToFile(std::string localPath);
private:
    std::vector<Entry> entrys;
};
}

#endif
