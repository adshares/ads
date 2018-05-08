#include "srvprinter.h"

#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "helper/servers.h"
#include "helper/ascii.h"
#include "helper/json.h"

SrvPrinter::SrvPrinter(const std::string& filepath) : DataPrinter(filepath) {
}

void SrvPrinter::printJson() {
    Helper::Servers servers(m_filepath.c_str());
    servers.load();

    Helper::ServersHeader header;
    std::vector<Helper::ServersNode> srv_nodes;
    header = servers.getHeader();
    srv_nodes = servers.getNodes();

    boost::property_tree::ptree pt;
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

    boost::property_tree::ptree nodes;
    for (auto &it : srv_nodes) {
        boost::property_tree::ptree node;

        node.put("public_key", Helper::ed25519_key2text(it.publicKey, 32));
        node.put("hash", Helper::ed25519_key2text((uint8_t*)&it.hash[0], 32));
        node.put("message_hash", Helper::ed25519_key2text(it.messageHash, 32));
        node.put("msg_id", it.messageId);
        node.put("msg_time", it.messageTime);
        node.put("balance", Helper::print_amount(it.weight));
        node.put("status", it.status);
        node.put("account_count", it.accountCount);
        node.put("port", it.port);

        struct in_addr ip_addr;
        ip_addr.s_addr = it.ipv4;
        node.put("ipv4", inet_ntoa(ip_addr));

        nodes.push_back(std::make_pair("", node));
    }
    pt.add_child("nodes", nodes);
    boost::property_tree::write_json(std::cout, pt, true);
}

void SrvPrinter::printString() {

}
