#include "msglistparser.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace Parser {

MsglistParser::MsglistParser(uint32_t path) : m_filePath(""){
    char filepath[64];
    sprintf(filepath,"blk/%03X/%05X/msglist.dat", path>>20, path&0xFFFFF);
    m_filePath = filepath;
}

MsglistParser::MsglistParser(const char* filepath) : m_filePath(filepath) {
}

bool MsglistParser::save(const char* filepath) {
    if (filepath) {
        m_filePath = filepath;
    }

    std::ofstream file;
    file.exceptions(std::ofstream::failbit | std::ofstream::badbit | std::ifstream::eofbit);
    try {
        file.open(m_filePath, std::ofstream::out | std::ofstream::binary);

        file.write((char*)&m_header, sizeof(m_header));
        for(auto &it : m_msgList) {
            file.write((char*)&it, sizeof(MessageRecord));
        }

        uint32_t hashesSize = m_hashes.size();
        file.write((char*)&hashesSize, sizeof(hashesSize));
        for (auto &it : m_hashes) {
            file.write((char*)&it, sizeof(hash_s));
        }
    } catch (std::exception&) {
        return false;
    }

    return true;
}

MessageListHeader MsglistParser::getHeader() {
    return m_header;
}

uint8_t* MsglistParser::getHeaderHash() {
    return m_header.message_hash;
}

std::vector<MessageRecord> MsglistParser::getMessageList() {
    return m_msgList;
}

bool MsglistParser::load(const char *filepath) {
    if (filepath) {
        m_filePath = filepath;
    }

    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    try {
        file.open(m_filePath, std::ifstream::in | std::ifstream::binary);

        file.read((char*)&m_header, sizeof(m_header));
        for (unsigned int i = 0; i<m_header.num_of_msg; ++i) {
            MessageRecord item;
            file.read((char*)&item, sizeof(item));
            m_msgList.push_back(item);
        }

        uint32_t hashesSize;
        file.read((char*)&hashesSize, sizeof(hashesSize));

        for (unsigned int i=0; i<hashesSize; ++i) {
            hash_s hash;
            file.read((char*)&hash, sizeof(hash));
            m_hashes.push_back(hash);
        }
    } catch (std::exception&) {
        return false;
    }

    return true;
}

MsgIterator MsglistParser::begin() {
    return m_msgList.begin();
}

MsgIterator MsglistParser::end() {
    return m_msgList.end();
}

bool MsglistParser::isEmpty() {
    return m_msgList.empty();
}

MsgIterator MsglistParser::get(unsigned int id) {
    if (id <= m_msgList.size()) {
        return m_msgList.begin() + (id - 1);
    }
    return m_msgList.end();
}

uint32_t MsglistParser::getHashesTotalSize() {
    return m_hashes.size();
}

HashIterator MsglistParser::h_begin() {
    return m_hashes.begin();
}

HashIterator MsglistParser::h_end() {
    return m_hashes.end();
}



}
