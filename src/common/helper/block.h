#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <openssl/sha.h>
#include "default.hpp"
#include "helper/serversheader.h"

namespace Helper {

class Block {
public:
    Block() = default;
    bool readDataFromHeaderFile();
    bool updateLastHeader();
    void loadFromHeader(const header_t&);
    header_t getHeader();
    ServersHeader& getData();
private:
    bool checkHeader(uint32_t expectedTime, uint32_t readTime);
    void hashnow();
    ServersHeader m_data{};
};

}

#endif
