#ifndef INXPRINTER_H
#define INXPRINTER_H

#include "dataprinter.h"

class InxPrinter : public DataPrinter {
public:
    InxPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};


#endif // INXPRINTER_H
