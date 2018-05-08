#include "undoprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "helper/ascii.h"
#include "helper/json.h"

UndoPrinter::UndoPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void UndoPrinter::printJson() {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    UndoHeader header;
    std::map<uint32_t, user_t> users;
    uint32_t no_of_users = 0;

    try {
        file.open(m_filepath, std::ifstream::in | std::ifstream::binary);
        file.seekg(0, std::ios_base::end);
        int size = file.tellg();
        file.seekg(0, std::ios_base::beg);

        file.read((char*)&header, sizeof(header));
        size -= sizeof(header);

        while (size > (int)sizeof(no_of_users)) {
            uint32_t user_id;
            user_t user{};
            file.read((char*)&user_id, sizeof(uint32_t));
            file.read((char*)&user, sizeof(user_t));
            users[user_id] = user;
            size -= (sizeof(uint32_t) + sizeof(user_t));
        }
        file.read((char*)&no_of_users, sizeof(no_of_users));
        file.close();
    } catch (std::exception& e) {
        throw std::runtime_error(e.what());
    }

    boost::property_tree::ptree pt;

    pt.put("checksum", Helper::ed25519_key2text((uint8_t*)header.checksum, 32));
    pt.put("balance", Helper::print_amount(header.weight));
    pt.put("fee", Helper::print_amount(header.fee));
    pt.put("msg_hash", Helper::ed25519_key2text(header.msg_hash, 32));
    pt.put("time", header.msg_time);

    boost::property_tree::ptree users_tree;
    for (auto &it : users) {
        boost::property_tree::ptree user;
        user.put("user_id", it.first);
        user.put("msid", it.second.msid);
        user.put("time", it.second.time);
        user.put("public_key", Helper::ed25519_key2text(it.second.pkey, 32));
        user.put("hash", Helper::ed25519_key2text(it.second.hash, 32));
        user.put("txn_local_time", it.second.lpath);
        user.put("user", it.second.user);
        user.put("node", it.second.node);
        user.put("status", it.second.stat);
        user.put("txn_incomming_time", it.second.rpath);
        user.put("balance", Helper::print_amount(it.second.weight));
        user.put("checksum", Helper::ed25519_key2text((uint8_t*)it.second.csum, 32));
        users_tree.push_back(std::make_pair("", user));
    }
    pt.add_child("users", users_tree);
    pt.put("no_of_users", no_of_users);
    boost::property_tree::write_json(std::cout, pt, true);
}

void UndoPrinter::printString() {

}
