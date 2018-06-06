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


namespace Helper {

Servers::Servers() {
    m_filePath = "";
}

Servers::Servers(const char* filePath)
    : m_filePath(filePath) {
}

void Servers::load(const char* filePath) {
    if (filePath) {
        m_filePath = filePath;
    }

    if (m_filePath.empty()) {
        return;
    }

    std::ifstream file(m_filePath, std::ifstream::in | std::ifstream::binary);
    if (file.is_open()) {
        file.read((char*)&m_header, sizeof(m_header));
        for (unsigned int i=0; i<m_header.nodesCount; ++i) {
            ServersNode node;
            file.read((char*)&node, sizeof(node));
            m_nodes.push_back(node);
        }
        file.close();
    }
}

bool Servers::loadHeader() {
    if (m_filePath.empty()) {
        char filePath[64];
        if (!m_header.ttime) {
            sprintf(filePath, "blk/header.hdr");
        } else {
            sprintf(filePath, "blk/%03X/%05X/header.hdr",m_header.ttime>>20, m_header.ttime&0xFFFFF);
        }
        m_filePath = filePath;
    }

    uint32_t check = m_header.ttime;
    std::ifstream file(m_filePath, std::ifstream::in | std::ifstream::binary);
    if (file.is_open()) {
        file.read((char*)&m_header, sizeof(m_header));
        file.close();
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
        sprintf(filePath, "blk/%03X.now", m_header.ttime>>20);
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

bool Servers::getMsglHashTree(uint16_t svid,uint32_t msid,uint32_t msg_number,std::vector<hash_s>& hashes) {
    if (!msg_number) {
        return false;
    }

    if (!loadHeader() && (m_header.messageCount == 0)) {
        return false;
    }

    Parser::MsglistParser parser(m_header.ttime);
    if (!parser.load() || parser.isEmpty()) {
//        DLOG("ERROR %s not found\n",filename);
        return false;
    }

    uint16_t msg_svid = 0;
    uint32_t msg_msid = 0;
    hash_s msg_hash;
    Parser::MsgIterator it = parser.begin() + (msg_number - 1);
    msg_svid = it->node_id;
    msg_msid = it->node_msid;

    if (msg_svid != svid || msg_msid != msid) {
//        DLOG("ERROR bad index %d %04X:%08X <> %04X:%08X\n", mnum, svid,msid, msg_svid, msg_msid);
        return false;
    }

    if (msg_number%2 == 0) {
        --it;
        memcpy(&msg_hash, it->hash, sizeof(msg_hash));
        hashes.push_back(msg_hash);
    } else {
        ++it;
        if (it != parser.end()) {
            memcpy(&msg_hash, it->hash, sizeof(msg_hash));
            hashes.push_back(msg_hash);
        }
    }

    --msg_number;
    uint32_t htot = parser.getHashesTotalSize();
    hashtree tree;
    if (htot > 0) {
        std::vector<uint32_t> add;
        tree.hashpath((msg_number)/2, (m_header.messageCount+1)/2, add);
        Parser::HashIterator hIt;
        for (auto n : add) {
            hIt = parser.h_begin() + n;
            if (hIt == parser.h_end()) {
                it = parser.end() - 1;
                hash_s lastTxnHash;
                std::copy(it->hash, it->hash+32, lastTxnHash.hash);
                hashes.push_back(lastTxnHash);
            } else {
                hashes.push_back(*hIt);
            }
        }
    }

    //DEBUG confirm hash
    hash_t nhash;
    tree.hashpathrun(nhash,hashes);
    if(memcmp(m_header.msgHash, nhash,32)) {
//        DLOG("HASHTREE failed (path len:%d) to get msghash\n",(int)hashes.size());
        return false;
    }
    hash_s* hashmin=(hash_s*)m_header.minHash;
    hashes.push_back(*hashmin);
    tree.addhash(nhash, m_header.minHash);
    if(memcmp(m_header.nowHash, nhash, 32)) {
//        DLOG("HASHTREE failed (path len:%d) to get nowhash\n",(int)hashes.size());
        return false;
    }

    return true;
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
