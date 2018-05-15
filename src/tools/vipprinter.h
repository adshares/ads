#ifndef VIPPRINTER_H
#define VIPPRINTER_H

#include "dataprinter.h"

namespace {
struct VipItem {
    uint16_t server_id;
    uint8_t public_key[32];
}__attribute__((packed));
}

class VipPrinter : public DataPrinter {
public:
    VipPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // VIPPRINTER_H
