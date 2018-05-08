#include "msglistprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "parser/msglistparser.h"
#include "helper/ascii.h"

MsgListPrinter::MsgListPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void MsgListPrinter::printJson() {
    Parser::MsglistParser parser(m_filepath.c_str());
    if (!parser.load()) {
        throw std::runtime_error("Can't open msglist.dat file");
    }

    if (parser.isEmpty()) {
        throw std::runtime_error("Empty file msglist.dat");
    }

    boost::property_tree::ptree pt;
    MessageListHeader header = parser.getHeader();
    pt.put("message_count", header.num_of_msg);
    pt.put("message_hash", Helper::ed25519_key2text(header.message_hash, 32));

    boost::property_tree::ptree messages;
    for (auto it = parser.begin(); it != parser.end(); ++it) {
        boost::property_tree::ptree msg;
        msg.put("node_id", it->node_id);
        msg.put("node_msid", it->node_msid);
        msg.put("hash", Helper::ed25519_key2text(it->hash, 32));
        messages.push_back(std::make_pair("", msg));
    }
    pt.add_child("messages", messages);

    boost::property_tree::ptree hashes;
    for(auto it = parser.h_begin(); it != parser.h_end(); ++it) {
        boost::property_tree::ptree hash;
        hash.put("", Helper::ed25519_key2text(it->hash, 32));
        hashes.push_back(std::make_pair("", hash));
    }
    pt.add_child("hashes", hashes);

    boost::property_tree::write_json(std::cout, pt, true);

}

void MsgListPrinter::printString() {

}
