#include "msgprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>

#include "abstraction/interfaces.h"
#include "command/factory.h"
#include "helper/ascii.h"

MsgPrinter::MsgPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void MsgPrinter::printJson() {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    Header header;

    try {
        file.open(m_filepath, std::ifstream::in | std::ifstream::binary);
        file.read((char*)&header, sizeof(header));

        uint32_t length  = header.type_and_length >> 8;
        if (length < sizeof(header)) {
            throw std::runtime_error("Bad length");
        }
        length -= sizeof(header);

        uint8_t buffer[length];
        file.read((char*)&buffer, sizeof(buffer));
        file.close();

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
        throw std::runtime_error(e.what());
    }
}

void MsgPrinter::printString() {

}
