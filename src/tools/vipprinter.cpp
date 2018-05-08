#include "vipprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>

#include "helper/ascii.h"
#include "helper/json.h"

VipPrinter::VipPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void VipPrinter::printJson() {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    std::vector<VipItem> vipList;

    try {
        file.open(m_filepath, std::ifstream::in | std::ifstream::binary);
        file.seekg(0, std::ios_base::end);
        int size = file.tellg();
        file.seekg(0, std::ios_base::beg);
        if (size%sizeof(VipItem) != 0) {
            std::cout<<"Incorrect size of file "<<size<<std::endl;
        }
        while (size > 0) {
            VipItem item{};
            file.read((char*)&item, sizeof(VipItem));
            vipList.push_back(item);
            size -= sizeof(VipItem);
        }
        file.close();
    } catch (std::exception& e) {
        throw std::runtime_error(e.what());
    }

    boost::property_tree::ptree pt;
    pt.put("filename_hash", boost::filesystem::basename(m_filepath));
    boost::property_tree::ptree vips_tree;
    for (auto &it : vipList) {
        boost::property_tree::ptree vip_tree;
        vip_tree.put("server_id", it.server_id);
        vip_tree.put("public_key", Helper::ed25519_key2text(it.public_key, sizeof(VipItem::public_key)));
        vips_tree.push_back(std::make_pair("", vip_tree));
    }
    pt.add_child("vip_keys", vips_tree);
    boost::property_tree::write_json(std::cout, pt, true);
}

void VipPrinter::printString() {

}
