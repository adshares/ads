#ifndef MSGPRINTER_H
#define MSGPRINTER_H

#include <stdint.h>

#include "dataprinter.h"

namespace {
struct Header {
    uint32_t type_and_length;
    uint8_t signature[64];
    uint16_t svid;
    uint32_t msid;
    uint32_t timestamp;
}__attribute__((packed));
}
class MsgPrinter : public DataPrinter {
public:
    MsgPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // MSGPRINTER_H
