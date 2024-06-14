#ifndef __DECOMPRESS_H__
#define __DECOMPRESS_H__

#include <vector>
#include <string>

#include "entry.h"

namespace Hdc {
class Decompress
{
public:
    Decompress(std::string tarPath)
        : tarPath(tarPath){}
    ~Decompress() {}

    bool DecompressToLocal(std::string decPath);

private:
    /* bool ReadData(); */

    std::vector<Entry> entrys;
    std::string tarPath;
};

}
#endif
