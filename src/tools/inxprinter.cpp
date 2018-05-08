#include "inxprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

InxPrinter::InxPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void InxPrinter::printJson() {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);

    boost::property_tree::ptree pt;
    boost::property_tree::ptree paths_tree;
    try {
        file.open(m_filepath, std::ifstream::in | std::ifstream::binary);
        file.seekg(0, std::ios_base::end);
        int size = file.tellg();
        file.seekg(0, std::ios_base::beg);
        if (size%sizeof(uint32_t) != 0) {
            std::cout<<"Incorrect size of file "<<size<<std::endl;
        }
        int id = 0;
        while (size > 0) {
            boost::property_tree::ptree path_tree;
            uint32_t path;
            file.read((char*)&path, sizeof(uint32_t));
            path_tree.put("", path);
            size -= sizeof(uint32_t);
            paths_tree.push_back(std::make_pair(std::to_string(id++), path_tree));
        }
        file.close();
    } catch (std::exception& e) {
        throw std::runtime_error(e.what());
    }
    pt.add_child("paths", paths_tree);
    boost::property_tree::write_json(std::cout, pt, true);
}

void InxPrinter::printString() {

}
