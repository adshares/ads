///TODO: uncomment logs when new log mechanism done
#include "servers.h"

#include <fstream>
#include <iostream>
#include "default.hpp"
#include "hash.hpp"

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

bool Servers::getMsglHashTree(uint16_t svid,uint32_t msid,uint32_t mnum,std::vector<hash_s>& hashes) {
    if (!mnum) {
        return false;
    }

    if (!m_header.messageCount && !loadHeader()) {
        return false;
    }

    char filename[64];
    sprintf(filename,"blk/%03X/%05X/msglist.dat", m_header.ttime>>20,m_header.ttime&0xFFFFF);
    std::ifstream file(filename, std::ifstream::in | std::ifstream::binary);
    if (!file.is_open()) {
//        DLOG("ERROR %s not found\n",filename);
        return false;
    }

    if ((--mnum)%2) {
        HashSingleVariant hash;
        file.seekg(4+32+(2+4+32)*(mnum)-32);
        file.read((char*)&hash, sizeof(hash));
        if(hash.svid!=svid || hash.msid!=msid) {
//            DLOG("ERROR %s bad index %d %04X:%08X <> %04X:%08X\n",filename,mnum,svid,msid,  tmp.svid,tmp.msid);
            file.close();
            return false;
        }
        hashes.push_back(hash.ha);
    } else {
        HashDoubleVariant hash;
        file.seekg(4+32+(2+4+32)*(mnum));
        file.read((char*)&hash, sizeof(hash));
        if(hash.svid1!=svid || hash.msid1!=msid) {
//            DLOG("ERROR %s bad index %d %04X:%08X <> %04X:%08X\n",filename,mnum,svid,msid, tmp.svid1,tmp.msid1);
            file.close();
            return false;
        }
        if(mnum<m_header.messageCount-1) {
//            DLOG("HASHTREE start %d + %d [max:%d]\n",mnum,mnum+1,m_header.messageCount);
            hashes.push_back(hash.ha2);
        } else {
//            DLOG("HASHTREE start %d [max:%d]\n",mnum,m_header.messageCount);
        }
    }

    uint32_t htot = 0;
    file.seekg(4+32+(2+4+32)* m_header.messageCount);
    file.read((char*)&htot, sizeof(htot));
    std::vector<uint32_t> add;
    hashtree tree;
    tree.hashpath(mnum/2, (m_header.messageCount+1)/2, add);
    for(auto n : add) {
//        DLOG("HASHTREE add %d\n",n);
        assert(n<htot);
        file.seekg(4+32+(2+4+32)*m_header.messageCount+4+32*n);
        hash_s phash;
        file.read((char*)&phash.hash, 32);
        hashes.push_back(phash);
    }
    file.close();

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

}
