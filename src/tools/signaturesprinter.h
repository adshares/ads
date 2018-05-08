#ifndef SIGNATURESPRINTER_H
#define SIGNATURESPRINTER_H

#include "dataprinter.h"

class SignaturesPrinter : public DataPrinter {
public:
    SignaturesPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // SIGNATURESPRINTER_H
