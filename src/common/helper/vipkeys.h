#ifndef VIPKEYS_H
#define VIPKEYS_H

#include <string>
#include <memory>
#include "default.hpp"

namespace Helper {

class VipKeys {
public:
    VipKeys() = default;
    void loadFromFile(const std::string& fileName);
    void loadFromBuffer(std::unique_ptr<char[]> buffer, u_int32_t length);
    bool checkVipKeys(uint8_t* compareHash, int num) const;
    bool storeVipKeys(const std::string& fileName) const;
    char* getVipKeys() const;
    uint32_t getLength() const;
    void reset();
private:
    std::unique_ptr<char[]> m_buffer;
    uint32_t m_length{};
};

}
#endif
