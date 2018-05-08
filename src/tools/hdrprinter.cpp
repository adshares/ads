#include "hdrprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>

#include "abstraction/interfaces.h"
#include "command/factory.h"
#include "helper/ascii.h"

HdrPrinter::HdrPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void HdrPrinter::printJson() {

}

void HdrPrinter::printString() {

}
