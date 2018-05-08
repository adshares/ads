#include "jsonprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>

#include "abstraction/interfaces.h"
#include "command/factory.h"
#include "helper/ascii.h"

JsonPrinter::JsonPrinter(const std::string& filepath) : m_filepath(filepath){
}

void JsonPrinter::printJson() {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    Header header;

    try {
        file.open(m_filepath, std::ifstream::in | std::ifstream::binary);
        std::cout<<"Reading header..."<<std::endl;
        file.read((char*)&header, sizeof(header));

        uint32_t length  = header.type_and_length >> 8;
        if (length < sizeof(header)) {
            throw std::runtime_error("Bad length");
        }
        length -= sizeof(header);

        std::cout<<"Reading data..."<<std::endl;
        uint8_t buffer[length];
        file.read((char*)&buffer, sizeof(buffer));
        file.close();

        std::cout<<"Printing result..."<<std::endl;
        boost::property_tree::ptree pt;
        // print header
        pt.put("type", (header.type_and_length & 0xFF));
        pt.put("signature", Helper::ed25519_key2text(header.signature, sizeof(header.signature))); // use key 2 text
        pt.put("svid", header.svid);
        pt.put("msid", header.msid);
        pt.put("now", header.timestamp);

        //print transactions
        std::unique_ptr<IBlockCommand> command;
        boost::property_tree::ptree transactions;
        uint8_t *ptr = buffer;
        while (length > 0) {
            command = command::factory::makeCommand(*ptr);
            if (command) {
                boost::property_tree::ptree txn;
                command->setData((char*)ptr);
                int size = command->getDataSize()  + command->getAdditionalDataSize() + command->getSignatureSize();
                command->txnToJson(txn);
                transactions.push_back(std::make_pair("", txn));
                ptr += size;
                length -= size;
            } else {
                break;
            }
        }
        pt.add_child("transactions", transactions);
        boost::property_tree::write_json(std::cout, pt, true);
        if (length > 0) {
            std::cout<<"Unsupported transaction, result might be not complete"<<std::endl;
        }
    } catch (std::exception& e) {
        throw e;
    }
}

void JsonPrinter::printString() {

}
