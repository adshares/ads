#include "msgprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>

#include "abstraction/interfaces.h"
#include "command/factory.h"
#include "helper/ascii.h"
#include <openssl/sha.h>

MsgPrinter::MsgPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void MsgPrinter::printJson() {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    Header      header;
    EndHeader   endHeader;

    try {
        auto readed = 0u;
        file.open(m_filepath, std::ifstream::in | std::ifstream::binary);
        file.read((char*)&header, sizeof(header));

        uint32_t length = header.lenght;
        length -= sizeof(header);

        uint8_t buffer[length];
        file.read((char*)&buffer, sizeof(buffer));
        file.read((char*)&endHeader, sizeof(EndHeader));

        readed += sizeof(header);
        readed += sizeof(buffer);
        readed += sizeof(EndHeader);

        boost::property_tree::ptree pt;
        // print header
        pt.put("type", (header.type));
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
                int size = command->getBlockMessageSize();
                command->txnToJson(txn);
                transactions.push_back(std::make_pair("", txn));
                ptr += size;
                length -= size;
            } else {
                break;
            }
        }
        pt.add_child("transactions", transactions);        

        pt.put("hash", Helper::ed25519_key2text(endHeader.hash, sizeof(endHeader.hash))); // use key 2 text
        pt.put("mnum", endHeader.mnum);
        pt.put("tmax", endHeader.tmax);
        pt.put("ttot", endHeader.ttot);

        std::vector<hash_s> hashes;

        boost::property_tree::ptree hashvector;

        for(auto i=0u;i<endHeader.tmax;i++)
        {
            hash_s      hash;
            uint32_t    pos[1];

            file.read((char*)&pos, sizeof(pos));
            file.read((char*)&hash, sizeof(hash_s));
            readed += sizeof(pos) + sizeof(hash_s);
            hashes.push_back((hash));

            boost::property_tree::ptree hashTree;
            hashTree.put("", Helper::ed25519_key2text((uint8_t*)&hash, sizeof(hash)));
            hashvector.push_back(std::make_pair("", hashTree)); // use key 2 text
        }

        while(endHeader.ttot > readed)
        {
            hash_s      hash;
            file.read((char*)&hash, sizeof(hash_s));
            readed += sizeof(hash_s);
            boost::property_tree::ptree hashTree;
            hashTree.put("-", Helper::ed25519_key2text((uint8_t*)&hash, sizeof(hash)));
            hashvector.push_back(std::make_pair("-", hashTree)); // use key 2 text
            hashes.push_back((hash));
        }

        pt.add_child("hashes", hashvector);

        file.close();

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
