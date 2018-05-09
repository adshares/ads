#ifndef UNDOPRINTER_H
#define UNDOPRINTER_H

#include "dataprinter.h"

namespace {
struct UndoHeader {
    uint64_t checksum[4];
    int64_t weight;
    int64_t fee;
    uint8_t msg_hash[32];
    uint32_t msg_time;
}__attribute__((packed));
}

class UndoPrinter : public DataPrinter {
public:
    UndoPrinter(const std::string &filepath);
    virtual void printJson() override;
    virtual void printString() override;
};

#endif // UNDOPRINTER_H
