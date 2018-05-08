#include "hdrprinter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "helper/servers.h"
#include "helper/ascii.h"
#include "helper/json.h"

HdrPrinter::HdrPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void HdrPrinter::printJson() {
    Helper::Servers servers(m_filepath.c_str());
    if (!servers.loadHeader()) {
        throw std::runtime_error("Can't load header");
    }

    boost::property_tree::ptree pt;
    Helper::ServersHeader header = servers.getHeader();
    pt.put("version", header.version);
    pt.put("timestamp", header.ttime);
    pt.put("message_count", header.messageCount);
    pt.put("nodes_count", header.nodesCount);
    pt.put("dividend_balance", Helper::print_amount(header.dividendBalance));

    pt.put("oldhash", Helper::ed25519_key2text(header.oldHash, 32));
    pt.put("minhash", Helper::ed25519_key2text(header.minHash, 32));
    pt.put("msghash", Helper::ed25519_key2text(header.msgHash, 32));
    pt.put("nodhash", Helper::ed25519_key2text(header.nodHash, 32));
    pt.put("viphash", Helper::ed25519_key2text(header.vipHash, 32));
    pt.put("nowhash", Helper::ed25519_key2text(header.nowHash, 32));

    pt.put("vote_yes", header.voteYes);
    pt.put("vote_no", header.voteNo);
    pt.put("vote_total", header.voteTotal);

    boost::property_tree::write_json(std::cout, pt, true);
}

void HdrPrinter::printString() {

}
