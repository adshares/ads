#ifndef SRVPRINTER_H
#define SRVPRINTER_H

#include "dataprinter.h"

class SrvPrinter : public DataPrinter {
public:
    SrvPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // SRVPRINTER_H
