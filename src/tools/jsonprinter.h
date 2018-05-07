#ifndef JSONPRINTER_H
#define JSONPRINTER_H

#include <string>
#include <stdint.h>

struct Header {
    uint32_t type_and_length;
    uint8_t signature[64];
    uint16_t svid;
    uint32_t msid;
    uint32_t timestamp;
}__attribute__((packed));

class JsonPrinter
{
public:
    JsonPrinter(const std::string &filepath);
    void printJson();
    void printString();

private:
    std::string m_filepath;
};

#endif // JSONPRINTER_H
