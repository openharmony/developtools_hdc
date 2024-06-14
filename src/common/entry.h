#ifndef __ENTRY_H__
#define __ENTRY_H__

#include <vector>

#include "header.h"

namespace Hdc {
class Entry
{
public:
    Entry(std::string path);
    Entry(uint8_t data[512]);
    ~Entry() {}

    bool IsFinish() {
        return this->need_size == 0;
    }

    bool IsInvalid() {
        return this->header.IsInvalid();
    }

    void AddData(uint8_t *data, size_t len);
    size_t Size() {
        return header.Size();
    }

    bool SaveToFile(std::string prefixPath);
    bool WriteToTar(std::ofstream &file);

private:
    Header header;
    size_t need_size;
    std::vector<uint8_t> data;
};

}
#endif
