#include "hlogprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "helper/ascii.h"
#include "helper/json.h"
#include "helper/hlog.h"

HlogPrinter::HlogPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void HlogPrinter::printJson() {
    boost::property_tree::ptree pt;
    Helper::Hlog hlog(pt, (char*)m_filepath.c_str());
    boost::property_tree::write_json(std::cout, pt, true);
}

void HlogPrinter::printString() {

}
