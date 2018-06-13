#include "vipkeys.h"
#include "hash.hpp"
#include "ascii.h"
#include <fstream>

namespace Helper {

void VipKeys::loadFromFile(const std::string& fileName) {
    std::ifstream file(fileName, std::ifstream::in | std::ifstream::binary);
    if(file.is_open()) {
        file.seekg(0, file.end);
        m_length = file.tellg();
        file.seekg(0, file.beg);

        m_buffer = std::make_unique<char[]>(m_length);
        file.read(m_buffer.get(), m_length);

        file.close();
    }
}

void VipKeys::loadFromBuffer(std::unique_ptr<char[]> buffer, u_int32_t length) {
    m_buffer = std::move(buffer);
    m_length = length;
}

char* VipKeys::getVipKeys() const {
    return m_buffer.get();
}

uint32_t VipKeys::getLength() const {
    return m_length;
}

bool VipKeys::checkVipKeys(uint8_t* compareHash, int num) const {
    hash_t newhash= {};
    hashtree tree(NULL); //FIXME, waste of space

    uint8_t* vipKeys = (uint8_t*)m_buffer.get();
    vipKeys+=2;
    for(int n=0; n<num; n++,vipKeys+=32+2) {
        tree.addhash(newhash, vipKeys);
    }
    return(!memcmp(newhash, compareHash, 32));
}

bool VipKeys::storeVipKeys(const std::string& fileName) const {
    std::ofstream file(fileName, std::ofstream::binary | std::ofstream::out);

    if(file.is_open()) {
        file.write(m_buffer.get(), m_length);
        fprintf(stderr, "INFO, stored VIP keys in %s\n", fileName.c_str());
        file.close();
        return true;
    }
    else {
        fprintf(stderr, "ERROR opening %s\n", fileName.c_str());
        return false;
    }
}

void VipKeys::reset() {
    m_length = 0;
    m_buffer.reset();
}

}
