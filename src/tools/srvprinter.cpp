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
    header.toJson(pt);
    boost::property_tree::ptree nodes;
    for (auto &it : srv_nodes) {
        boost::property_tree::ptree node;

        it.toJson(node);
        nodes.push_back(std::make_pair("", node));
    }
    pt.add_child("nodes", nodes);
    boost::property_tree::write_json(std::cout, pt, true);
}

void SrvPrinter::printString() {

}
