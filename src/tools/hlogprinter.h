#ifndef HLOGPRINTER_H
#define HLOGPRINTER_H

#include "dataprinter.h"

class HlogPrinter : public DataPrinter {
public:
    HlogPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // HLOGPRINTER_H
