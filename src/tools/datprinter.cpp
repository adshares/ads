#include "datprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "helper/ascii.h"
#include "helper/json.h"

DatPrinter::DatPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void DatPrinter::printJson() {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    std::vector<user_t> users;

    try {
        file.open(m_filepath, std::ifstream::in | std::ifstream::binary);
        file.seekg(0, std::ios_base::end);
        int size = file.tellg();
        file.seekg(0, std::ios_base::beg);
        if (size%sizeof(user_t) != 0) {
            std::cout<<"Incorrect size of file "<<size<<std::endl;
        }
        while (size > 0) {
            user_t item{};
            file.read((char*)&item, sizeof(user_t));
            users.push_back(item);
            size -= sizeof(user_t);
        }
        file.close();
    } catch (std::exception& e) {
        throw std::runtime_error(e.what());
    }

    boost::property_tree::ptree pt;
    boost::property_tree::ptree users_tree;
    for (auto &it : users) {
        boost::property_tree::ptree user_tree;
        user_tree.put("msid", it.msid);
        user_tree.put("time", it.time);
        user_tree.put("public_key", Helper::ed25519_key2text(it.pkey, 32));
        user_tree.put("hash", Helper::ed25519_key2text(it.hash, 32));
        user_tree.put("txn_local_time", it.lpath);
        user_tree.put("user", it.user);
        user_tree.put("node", it.node);
        user_tree.put("status", it.stat);
        user_tree.put("txn_incomming_time", it.rpath);
        user_tree.put("balance", Helper::print_amount(it.weight));
        user_tree.put("checksum", Helper::ed25519_key2text((uint8_t*)it.csum, 32));
        users_tree.push_back(std::make_pair("", user_tree));
    }
    pt.add_child("users", users_tree);
    boost::property_tree::write_json(std::cout, pt, true);
}

void DatPrinter::printString() {

}
