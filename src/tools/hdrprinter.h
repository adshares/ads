#ifndef HDRPRINTER_H
#define HDRPRINTER_H

#include "dataprinter.h"

class HdrPrinter : public DataPrinter {
public:
    HdrPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // HDRPRINTER_H
