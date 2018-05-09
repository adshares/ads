#ifndef MSGLISTPRINTER_H
#define MSGLISTPRINTER_H

#include "dataprinter.h"

class MsgListPrinter : public DataPrinter {
public:
    MsgListPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // MSGLISTPRINTER_H
