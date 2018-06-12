#include <cstdio>
#include <fstream>
#include "block.h"
#include "hash.hpp"

namespace Helper {

ServersHeader& Block::getData() {
    return m_data;
}

header_t Block::getHeader() {
    const ServersHeader& bd = m_data;
    header_t header{};
    header.now=bd.ttime;
    header.msg=bd.messageCount;
    header.nod=bd.nodesCount;
    header.div=bd.dividendBalance;
    memcpy(header.oldhash, bd.oldHash, SHA256_DIGEST_LENGTH);
    memcpy(header.msghash, bd.msgHash, SHA256_DIGEST_LENGTH);
    memcpy(header.nodhash, bd.nodHash, SHA256_DIGEST_LENGTH);
    memcpy(header.viphash, bd.vipHash, SHA256_DIGEST_LENGTH);
    memcpy(header.nowhash, bd.nowHash, SHA256_DIGEST_LENGTH);
    header.vok=bd.voteYes;
    header.vno=bd.voteNo;
    return header;
}

bool Block::readDataFromHeaderFile() {
    char fileName[64];
    if(!m_data.ttime) {
        sprintf(fileName,"blk/header.hdr");
    } else {
        sprintf(fileName,"blk/%03X/%05X/header.hdr", m_data.ttime>>20, m_data.ttime&0xFFFFF);
    }
    const uint32_t check = m_data.ttime;
    std::ifstream file(fileName, std::ifstream::in | std::ifstream::binary);
    if(file.is_open()) {
        file.read((char*)&m_data, sizeof(m_data));
        file.close();
        if(m_data.ttime == 0) {
            return false;
        }
        return checkHeader(check, m_data.ttime);
    }
    return false;
}

bool Block::checkHeader(uint32_t expectedTime, uint32_t readTime) {
    if(expectedTime && expectedTime!=readTime) {
        return false;
    }
    return true;
}

bool Block::updateLastHeader() {
    m_data.version=1;
    char filename[64]="blk/header.hdr";
    std::ofstream file(filename, std::ofstream::binary | std::ofstream::out);
    if(file.is_open()) {
        file.write((char*)&m_data, sizeof(m_data));
        file.close();
        return true;
    }
    return false;
}

void Block::hashnow() {
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, &m_data.ttime, sizeof(uint32_t));
    SHA256_Update(&sha256, &m_data.messageCount, sizeof(uint32_t));
    SHA256_Update(&sha256, &m_data.nodesCount, sizeof(uint32_t));
    SHA256_Update(&sha256, &m_data.dividendBalance, sizeof(uint32_t));
    SHA256_Final(m_data.nowHash, &sha256);
    hashtree tree(NULL); //FIXME, waste of space
    tree.addhash(m_data.nowHash, m_data.nodHash);
    tree.addhash(m_data.nowHash, m_data.vipHash);
    tree.addhash(m_data.nowHash, m_data.oldHash);
    memcpy(m_data.minHash, m_data.nowHash, 32); // add message hash as last hash to reduce hash path for transactions
    tree.addhash(m_data.nowHash, m_data.msgHash);
}

void Block::loadFromHeader(const header_t& header) {
    m_data.ttime=header.now;
    m_data.messageCount=header.msg;
    m_data.nodesCount=header.nod;
    m_data.dividendBalance=header.div;
    memcpy(m_data.oldHash, header.oldhash, SHA256_DIGEST_LENGTH);
    memcpy(m_data.msgHash, header.msghash, SHA256_DIGEST_LENGTH);
    memcpy(m_data.nodHash, header.nodhash, SHA256_DIGEST_LENGTH);
    memcpy(m_data.vipHash, header.viphash, SHA256_DIGEST_LENGTH);
    hashnow();
}

}
