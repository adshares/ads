#include "msglistprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>

#include "abstraction/interfaces.h"
#include "command/factory.h"
#include "helper/ascii.h"

MsgListPrinter::MsgListPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void MsgListPrinter::printJson() {

}

void MsgListPrinter::printString() {

}
