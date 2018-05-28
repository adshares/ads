#include "hdrprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "helper/servers.h"
#include "helper/ascii.h"
#include "helper/json.h"

HdrPrinter::HdrPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void HdrPrinter::printJson() {
    Helper::Servers servers(m_filepath.c_str());
    if (!servers.loadHeader()) {
        throw std::runtime_error("Can't load header");
    }

    boost::property_tree::ptree pt;
    Helper::ServersHeader header = servers.getHeader();
    header.toJson(pt);

    boost::property_tree::write_json(std::cout, pt, true);
}

void HdrPrinter::printString() {

}
