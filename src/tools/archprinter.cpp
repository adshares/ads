#include "archprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "hdrprinter.h"
#include "signaturesprinter.h"
#include "hlogprinter.h"
#include "msgprinter.h"
#include "srvprinter.h"

#include "abstraction/interfaces.h"
#include "command/factory.h"
#include "helper/ascii.h"
#include "helper/blockfilereader.h"
#include "parser/msglistparser.h"


ArchPrinter::ArchPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void ArchPrinter::printJson() {

    std::unique_ptr<DataPrinter> printer;
    std::cout<<"header.hdr"<<std::endl;
    printer = std::make_unique<HdrPrinter>(m_filepath);
    printer->printJson();

    std::cout<<"servers.srv"<<std::endl;
    printer = std::make_unique<SrvPrinter>(m_filepath);
    printer->printJson();

    std::cout<<"signatures"<<std::endl;
    printer = std::make_unique<SignaturesPrinter>(m_filepath);
    printer->printJson();

    std::cout<<"hlog.hlg"<<std::endl;
    printer = std::make_unique<HlogPrinter>(m_filepath);
    printer->printJson();

    std::cout<<"msglist.dat"<<std::endl;
    this->printMsgList();


}

void ArchPrinter::printString() {

}

void ArchPrinter::printMsgList() {
    Helper::BlockFileReader reader(m_filepath.c_str(), "msglist.dat");
    MessageListHeader header;
    std::vector<MessageRecord> msglist;
    std::vector<hash_s> hashes;
    if (reader.isOpen())
    {
        reader.read((char*)&header, sizeof(header));
        for (unsigned int i=0; i<header.num_of_msg; ++i)
        {
            MessageRecord item;
            reader.read((char*)&item, sizeof(item));
            msglist.push_back(item);
        }
        uint32_t hashesSize;
        reader.read((char*)&hashesSize, sizeof(hashesSize));

        for (unsigned int i=0; i<hashesSize; ++i)
        {
            hash_s hash;
            reader.read((char*)&hash, sizeof(hash));
            hashes.push_back(hash);
        }

        boost::property_tree::ptree pt;
        pt.put("message_count", header.num_of_msg);
        pt.put("message_hash", Helper::ed25519_key2text(header.message_hash, 32));

        boost::property_tree::ptree messages;
        for (auto it = msglist.begin(); it != msglist.end(); ++it) {
            boost::property_tree::ptree msg;
            msg.put("node_id", it->node_id);
            msg.put("node_msid", it->node_msid);
            msg.put("hash", Helper::ed25519_key2text(it->hash, 32));
            messages.push_back(std::make_pair("", msg));
        }
        pt.add_child("messages", messages);

        boost::property_tree::ptree thashes;
        for (auto it = hashes.begin(); it != hashes.end(); ++it) {
            boost::property_tree::ptree hash;
            hash.put("", Helper::ed25519_key2text(it->hash, 32));
            thashes.push_back(std::make_pair("", hash));
        }
        pt.add_child("hashes", thashes);

        boost::property_tree::write_json(std::cout, pt, true);

    }


    for (auto it = msglist.begin(); it != msglist.end(); ++it)
    {
        char msgfilepath[64];
        snprintf(msgfilepath, sizeof(msgfilepath), "%02x_%04x_%08x.msg", 3, it->node_id, it->node_msid);
        printMsg(msgfilepath);
    }
}

void ArchPrinter::printMsg(const char *msgpath) {

    std::cout<<msgpath<<std::endl;
    Header      header;
    EndHeader   endHeader;
    Helper::BlockFileReader file(m_filepath.c_str());
    file.openFromArch(msgpath);
    if(file.isOpen())
    {
        auto readed = 0u;
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
    }
}

