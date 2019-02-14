///TODO: uncomment logs when new log mechanism done
#include "servers.h"

#include <fstream>
#include <iostream>
#include <arpa/inet.h>
#include "default.hpp"
#include "hash.hpp"
#include "command/pods.h"
#include "parser/msglistparser.h"
#include "helper/ascii.h"
#include "helper/json.h"
#include "helper/blocks.h"
#include "helper/blockfilereader.h"

namespace Helper {

Servers::Servers() {
    m_filePath = "";
}

Servers::Servers(const char* filePath)
    : m_filePath(filePath) {
}

Servers::~Servers(){
}

void Servers::load(const char* filePath) {
    if (filePath) {
        m_filePath = filePath;
    }

    if (m_filePath.empty()) {
        return;
    }

    Helper::BlockFileReader file(m_filePath.c_str());
    if (file.isOpen()) {
        file.read((char*)&m_header, sizeof(m_header));
        for (unsigned int i=0; i<m_header.nodesCount; ++i) {
            ServersNode node;
            file.read((char*)&node, sizeof(node));
            m_nodes.push_back(node);
        }
    }
}

bool Servers::loadHeader() {
    if (m_filePath.empty()) {
        char filePath[64];
        if (!m_header.ttime) {
            snprintf(filePath, sizeof(filePath), "blk/header.hdr");
        } else {
            Helper::FileName::getName(filePath, sizeof(filePath), m_header.ttime, "header.hdr");
        }
        m_filePath = filePath;
    }

    uint32_t check = m_header.ttime;

    Helper::BlockFileReader file(m_filePath.c_str());
    if (file.isOpen()) {
        file.read((char*)&m_header, sizeof(m_header));
        if (check && check != m_header.ttime) {
            return false;
        }
        return true;
    }
    return false;
}

bool Servers::loadNowhash() {
    if (m_filePath.empty()) {
        char filePath[64];
        snprintf(filePath, sizeof(filePath), "blk/%03X.now", m_header.ttime>>20);
        m_filePath = filePath;
    }

    std::ifstream file(m_filePath, std::ifstream::in | std::ifstream::binary);
    if (file.is_open()) {
        file.seekg(((m_header.ttime&0xFFFFF)/BLOCKSEC)*SHA256_DIGEST_LENGTH);
        file.read((char*)&m_header.nowHash, sizeof(m_header.nowHash));
        if (!file) {
            return false;
        }
        file.close();
    } else {
        return false;
    }
    return true;
}

void Servers::setNow(uint32_t time) {
    m_header.ttime = time;
}

uint8_t* Servers::getNowHash() {
    return m_header.nowHash;
}

ServersHeader Servers::getHeader() {
    return m_header;
}

std::vector<ServersNode> Servers::getNodes() {
    return m_nodes;
}

ServersNode Servers::getNode(unsigned int nodeId) {
    if (m_nodes.size() < nodeId){
        throw std::invalid_argument("Node Id over limit");
    }

    return m_nodes.at(nodeId);
}

unsigned int Servers::getNodesCount() {
    return m_nodes.size();
}

void ServersNode::toJson(boost::property_tree::ptree& node)
{
    char value[65];
    value[64]='\0';

    Helper::ed25519_key2text(value, publicKey, 32);
    node.put("public_key", value);
    Helper::ed25519_key2text(value, (uint8_t*)hash, 32);
    node.put("hash", value);
    Helper::ed25519_key2text(value, messageHash, 32);
    node.put("message_hash", value);
    node.put("msid", messageId);
    node.put("mtim", messageTime);
    node.put("balance", print_amount(weight));
    node.put("status", status);
    node.put("account_count", accountCount);
    node.put("port", port);

    struct in_addr ip_addr;
    ip_addr.s_addr = ipv4;
    node.put("ipv4", inet_ntoa(ip_addr));
}

}
