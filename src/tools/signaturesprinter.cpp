#include "signaturesprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "helper/ascii.h"
#include "helper/json.h"

SignaturesPrinter::SignaturesPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void SignaturesPrinter::printJson() {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    std::vector<SignatureItem> signatures;

    try {
        file.open(m_filepath, std::ifstream::in | std::ifstream::binary);
        file.seekg(0, std::ios_base::end);
        int size = file.tellg();
        file.seekg(0, std::ios_base::beg);
        if (size%sizeof(SignatureItem) != 0) {
            std::cout<<"Incorrect size of file "<<size<<std::endl;
        }
        while (size > 0) {
            SignatureItem item{};
            file.read((char*)&item, sizeof(SignatureItem));
            signatures.push_back(item);
            size -= sizeof(SignatureItem);
        }
        file.close();
    } catch (std::exception& e) {
        throw std::runtime_error(e.what());
    }

    boost::property_tree::ptree pt;
    boost::property_tree::ptree signs;
    for (auto &it : signatures) {
        boost::property_tree::ptree sign;
        sign.put("server_id", it.server_id);
        sign.put("signature", Helper::ed25519_key2text(it.signature, sizeof(SignatureItem::signature)));
        signs.push_back(std::make_pair("", sign));
    }
    pt.add_child("signatures", signs);
    boost::property_tree::write_json(std::cout, pt, true);
}

void SignaturesPrinter::printString() {

}

