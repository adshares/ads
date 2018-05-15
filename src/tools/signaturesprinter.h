#ifndef SIGNATURESPRINTER_H
#define SIGNATURESPRINTER_H

#include "dataprinter.h"

namespace {
struct SignatureItem {
    uint16_t server_id;
    uint8_t signature[64];
}__attribute__((packed));
}

class SignaturesPrinter : public DataPrinter {
public:
    SignaturesPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // SIGNATURESPRINTER_H
