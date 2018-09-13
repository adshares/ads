#ifndef ARCHPRINTER_H
#define ARCHPRINTER_H

#include "dataprinter.h"

class ArchPrinter : public DataPrinter {
public:
    ArchPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
private:
    virtual void printMsgList();
    virtual void printMsg(const char* msgpath);
};

#endif // ARCHPRINTER_H
