#include "signaturesprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>

#include "helper/ascii.h"
#include "helper/json.h"
#include "helper/blockfilereader.h"

SignaturesPrinter::SignaturesPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void SignaturesPrinter::printJson() {
    std::vector<SignatureItem> signatures;
    boost::property_tree::ptree pt;
    boost::property_tree::ptree signs;

    if(boost::filesystem::path(m_filepath).extension() == ".arch")
    {
        {
            Helper::BlockFileReader file(m_filepath.c_str(), "signatures.ok");
            if (file.isOpen())
            {
                int size = file.getSize();
                if (size%sizeof(SignatureItem) != 0) {
                    std::cout<<"Incorrect size of file "<<size<<std::endl;
                }
                while (size > 0) {
                    SignatureItem item{};
                    file.read((char*)&item, sizeof(SignatureItem));
                    signatures.push_back(item);
                    size -= sizeof(SignatureItem);
                }
            }
            for (auto &it : signatures) {
                boost::property_tree::ptree sign;
                sign.put("server_id", it.server_id);
                sign.put("signature", Helper::ed25519_key2text(it.signature, sizeof(SignatureItem::signature)));
                signs.push_back(std::make_pair("", sign));
            }
            pt.add_child("signatures_ok", signs);
        }
        {
            signatures.clear();
            Helper::BlockFileReader file(m_filepath.c_str(), "signatures.no");
            if (file.isOpen())
            {
                int size = file.getSize();
                if (size%sizeof(SignatureItem) != 0) {
                    std::cout<<"Incorrect size of file "<<size<<std::endl;
                }
                while (size > 0) {
                    SignatureItem item{};
                    file.read((char*)&item, sizeof(SignatureItem));
                    signatures.push_back(item);
                    size -= sizeof(SignatureItem);
                }
            }
            if (!signatures.empty()) {
                for (auto &it : signatures) {
                    boost::property_tree::ptree sign;
                    sign.put("server_id", it.server_id);
                    sign.put("signature", Helper::ed25519_key2text(it.signature, sizeof(SignatureItem::signature)));
                    signs.push_back(std::make_pair("", sign));
                }
                pt.add_child("signatures_no", signs);
            }
        }


    } else
    {
        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);

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

        for (auto &it : signatures) {
            boost::property_tree::ptree sign;
            sign.put("server_id", it.server_id);
            sign.put("signature", Helper::ed25519_key2text(it.signature, sizeof(SignatureItem::signature)));
            signs.push_back(std::make_pair("", sign));
        }
        pt.add_child("signatures", signs);
    }
    boost::property_tree::write_json(std::cout, pt, true);
}

void SignaturesPrinter::printString() {

}

