#include "signaturesprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "helper/ascii.h"
#include "helper/json.h"

SignaturesPrinter::SignaturesPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void SignaturesPrinter::printJson() {

    boost::property_tree::ptree pt;
    boost::property_tree::write_json(std::cout, pt, true);
}

void SignaturesPrinter::printString() {

}

