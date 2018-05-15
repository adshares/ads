#ifndef DATPRINTER_H
#define DATPRINTER_H

#include "dataprinter.h"

class DatPrinter : public DataPrinter {
public:
    DatPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // DATPRINTER_H
