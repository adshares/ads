#ifndef LOGPRINTER_H
#define LOGPRINTER_H

#include "dataprinter.h"

class LogPrinter : public DataPrinter {
public:
    LogPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // LOGPRINTER_H
