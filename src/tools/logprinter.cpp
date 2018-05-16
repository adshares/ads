#include "logprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "helper/ascii.h"
#include "helper/json.h"
#include "command/pods.h"
#include "default.hpp"
#include "helper/txsname.h"

LogPrinter::LogPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void LogPrinter::printJson() {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    std::vector<log_t> logsList;

    try {
        file.open(m_filepath, std::ifstream::in | std::ifstream::binary);
        file.seekg(0, std::ios_base::end);
        int size = file.tellg();
        file.seekg(0, std::ios_base::beg);
        if (size%sizeof(log_s) != 0) {
            std::cout<<"Incorrect size of file "<<size<<std::endl;
        }
        while (size > 0) {
            log_t item{};
            file.read((char*)&item, sizeof(item));
            logsList.push_back(item);
            size -= sizeof(item);
        }
        file.close();
    } catch (std::exception& e) {
        throw std::runtime_error(e.what());
    }

    boost::property_tree::ptree pt;
    boost::property_tree::ptree logs;
    for (auto &it : logsList) {
        boost::property_tree::ptree log;
        uint16_t txst = it.type&0xFF;
        log.put("time", it.time);
        log.put("type", it.type);
        log.put("type_no", txst);
        if (txst<TXSTYPE_MAX) {
            log.put("type_name", Helper::logname[txst]);
        }
        if (it.type & 0x4000) {
            log.put("type_status", "error");
        } else if(it.type & 0x8000) {
            log.put("type_status", "in");
        } else {
            log.put("type_status", "out");
        }
        log.put("node", it.node);
        log.put("msid", it.nmid);
        log.put("mpos", it.mpos);
        log.put("user", it.user);
        log.put("user_msid", it.umid);
        log.put("weight", Helper::print_amount(it.weight));
        log.put("info", it.info);
        log.put("info_txt", Helper::ed25519_key2text(it.info, sizeof(it.info)));
        tInfo info = *reinterpret_cast<tInfo*>(it.info);
        log.put("info.weight", Helper::print_amount(info.weight));
        log.put("info.deduct", Helper::print_amount(info.deduct));
        log.put("info.fee", Helper::print_amount(info.fee));
        log.put("info.stat", info.stat);
        log.put("info.pkey", Helper::ed25519_key2text(info.pkey, sizeof(info.pkey)));
        logs.push_back(std::make_pair("", log));
    }
    pt.add_child("logs", logs);
    boost::property_tree::write_json(std::cout, pt, true);
}

void LogPrinter::printString() {

}

