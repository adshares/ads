#include "getbroadcastmsg.h"

#include <iostream>
#include <fstream>
#include <sys/stat.h>

#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/hash.h"

GetBroadcastMsg::GetBroadcastMsg()
    : m_data{}, m_loadedFromLocal(false) {
    m_responseError = ErrorCodes::Code::eNone;
}

GetBroadcastMsg::GetBroadcastMsg(uint16_t abank, uint32_t auser, uint32_t block, uint32_t time)
    : m_data(abank, auser, block, time), m_loadedFromLocal(false) {
    m_responseError = ErrorCodes::Code::eNone;
}

GetBroadcastMsg::~GetBroadcastMsg() {
}

unsigned char* GetBroadcastMsg::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetBroadcastMsg::getResponse() {
    // only header
    return reinterpret_cast<unsigned char*>(&m_header);
}

void GetBroadcastMsg::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetBroadcastMsg::setResponse(char* response) {
    // only header
    m_header = *reinterpret_cast<decltype(m_header)*>(response);
}

int GetBroadcastMsg::getDataSize() {
    return sizeof(m_data.info);
}

int GetBroadcastMsg::getResponseSize() {
    // only header
    return sizeof(m_header);
}

unsigned char* GetBroadcastMsg::getSignature() {
    return m_data.sign;
}

int GetBroadcastMsg::getSignatureSize() {
    return sizeof(m_data.sign);
}

int GetBroadcastMsg::getType() {
    return TXSTYPE_BLG;
}

CommandType GetBroadcastMsg::getCommandType() {
    return CommandType::eReadingOnly;
}

uint32_t GetBroadcastMsg::getUserMessageId() {
    return 0;
}

void GetBroadcastMsg::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetBroadcastMsg::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return (ed25519_sign_open(getData(), getDataSize(), pk, getSignature()) == 0);
}

void GetBroadcastMsg::saveResponse(settings& /*sts*/) {
}

uint32_t GetBroadcastMsg::getUserId() {
    return m_data.info.src_user;
}

uint32_t GetBroadcastMsg::getBankId() {
    return m_data.info.src_node;
}

uint32_t GetBroadcastMsg::getTime() {
    return m_data.info.ttime;
}

int64_t GetBroadcastMsg::getFee() {
    return 0;
}

int64_t GetBroadcastMsg::getDeduct() {
    return 0;
}

bool GetBroadcastMsg::send(INetworkClient& netClient) {
    if (loadFromLocalPath()) {
        return true;
    }

    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("GetBroadcastMsg sending error\n");
        return false;
    }

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("GetBroadcastMsg reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("GetBroadcastMsg reading header error\n");
        return false;
    }

    ELOG("size %ud %08X\n", m_header.fileSize, m_data.info.block);

    unsigned char *readBuffer = new unsigned char[m_header.fileSize];
    if (!netClient.readData(readBuffer, m_header.fileSize)) {
        ELOG("GetBroadcastMsg reading error\n");
        delete[] readBuffer;
        return false;
    }

    readDataBuffer(readBuffer, m_header.fileSize);

    saveCopy(readBuffer, m_header.fileSize);

    delete[] readBuffer;
    return true;
}

uint32_t GetBroadcastMsg::getBlockTime() {
    return m_data.info.block;
}

std::string GetBroadcastMsg::toString(bool /*pretty*/) {
    return "";
}

void GetBroadcastMsg::toJson(boost::property_tree::ptree& ptree) {
    if (m_responseError) {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    } else {
        char blockhex[9];
        blockhex[8]='\0';
        sprintf(blockhex,"%08X", m_header.path);
        ptree.put("block_time_hex", blockhex);
        ptree.put("block_time", m_header.path);
        ptree.put("broadcast_count", m_response.size());
        ptree.put("log_file", (m_loadedFromLocal) ? "archive" : "new");
        if(m_response.size() > 0) {
            boost::property_tree::ptree blockTree;
            for (auto &it : m_response) {
                printBlg(it.first, it.second, blockTree);
            }

            ptree.add_child("broadcast", blockTree);
        }
    }
}

void GetBroadcastMsg::printBlg(GetBroadcastResponse& block, std::string& message, boost::property_tree::ptree &ptree) {
    char pkey[65];
    char hash[65];
    ed25519_key2text(pkey,block.data.pkey,32);
    ed25519_key2text(hash,block.data.hash,32);
    pkey[64]='\0';
    hash[64]='\0';
    uint16_t suffix=Helper::crc_acnt(block.info.src_node, block.info.src_user);
    char acnt[19];
    sprintf(acnt,"%04X-%08X-%04X", block.info.src_node, block.info.src_user, suffix);

    boost::property_tree::ptree blogentry;
    blogentry.put("block_time", m_header.path);
    blogentry.put("block_date", mydate(m_header.path));
    if (block.info.ttype != TXSTYPE_BRO) {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(ErrorCodes::Code::eIncorrectTransaction));
        return;
    }
    blogentry.put("node", block.info.src_node);
    blogentry.put("account",block.info.src_user);
    blogentry.put("address",acnt);
    blogentry.put("account_msid", block.info.msg_id);
    blogentry.put("time", block.info.ttime);
    blogentry.put("date",mydate(block.info.ttime));

    //transaction data
    std::stringstream tx_data;
    Helper::ed25519_key2text(tx_data, reinterpret_cast<unsigned char*>(&block.info), sizeof(block.info));
    blogentry.put("data", tx_data.str());

    //message
    tx_data.str("");
    Helper::ed25519_key2text(tx_data, (unsigned char*)message.c_str(), block.info.msg_length);
    blogentry.put("message", tx_data.str());

    //signature
    tx_data.str("");
    Helper::ed25519_key2text(tx_data, reinterpret_cast<unsigned char*>(&block.data.sign), sizeof(block.data.sign));
    blogentry.put("signature", tx_data.str());

    //hash
    tx_data.str("");
    Helper::ed25519_key2text(tx_data, reinterpret_cast<unsigned char*>(&block.data.hash), sizeof(block.data.hash));
    blogentry.put("input_hash", tx_data.str());

    //public key
    tx_data.str("");
    Helper::ed25519_key2text(tx_data, reinterpret_cast<unsigned char*>(&block.data.pkey), sizeof(block.data.pkey));
    blogentry.put("public_key", tx_data.str());

    bool result = this->checkSignature(block.data.sign, block.data.pkey);
    blogentry.put("verify", (result) ? "passed" : "failed");

    //tx_id
    char tx_id[64];
    sprintf(tx_id,"%04X:%08X:%04X", block.info.src_node, block.data.msid, block.data.mpos);
    blogentry.put("node_msid", block.data.msid);
    blogentry.put("node_mpos", block.data.mpos);
    blogentry.put("id",tx_id);

    blogentry.put("fee",print_amount(TXS_BRO_FEE(block.info.msg_length)));
    ptree.push_back(std::make_pair("", blogentry));
}

bool GetBroadcastMsg::loadFromLocalPath() {
    uint32_t blockTime = this->getBlockTime();
    if (!blockTime) {
        return false;
    }

    uint32_t path = blockTime-(blockTime%BLOCKSEC);
    char filename[64];
    sprintf(filename, "bro/%08X.bin", path);
    std::ifstream file(filename, std::ifstream::binary | std::ifstream::in);
    if (!file.is_open()) {
        m_loadedFromLocal = false;
        return false;
    }

    struct stat sstat;
    stat(filename, &sstat);
    int remainData = sstat.st_size;
    m_header.fileSize = remainData;
    m_header.path = path;
    m_loadedFromLocal = true;

    while (remainData > 0) {
        GetBroadcastResponse response{};
        file.read((char*)&response.info, sizeof(response.info));
        remainData -= sizeof(response.info);

        char message[response.info.msg_length];
        file.read((char*)message, response.info.msg_length);
        remainData -= response.info.msg_length;

        file.read((char*)&response.data, sizeof(response.data));
        remainData -= sizeof(response.data);

        m_response.push_back(std::make_pair(response, std::string(message, response.info.msg_length)));
    }
    file.close();

    return true;
}

void GetBroadcastMsg::saveCopy(unsigned char *dataBuffer, int size) {
    uint32_t blockTime = m_header.path;
    if (!blockTime || m_response.empty() || size <= 0) {
        return;
    }

    uint32_t path = blockTime - blockTime%BLOCKSEC;

    char filename[64];
    mkdir("bro",0755);
    sprintf(filename, "bro/%08X.bin", path);
    std::ofstream file(filename, std::ofstream::binary | std::ofstream::out);
    if (file.is_open()) {
        file.write(reinterpret_cast<char*>(dataBuffer), size);
        file.close();
    }
}

void GetBroadcastMsg::readDataBuffer(unsigned char* dataBuffer, int size) {
    unsigned char *bufferPtr = dataBuffer;

    while (size > 0) {
        GetBroadcastResponse response{};

        // read SendBroadcastInfo data
        memcpy(&response.info, bufferPtr, sizeof(response.info));
        bufferPtr += sizeof(response.info);
        size -= sizeof(response.info);

        // read Message, depends on SendBroadcastInfo msg_length field
        char message[response.info.msg_length];
        memcpy(message, bufferPtr, response.info.msg_length);
        bufferPtr += response.info.msg_length;
        size -= response.info.msg_length;

        // read rest of file
        memcpy(&response.data, bufferPtr, sizeof(response.data));
        bufferPtr += sizeof(response.data);
        size -= sizeof(response.data);

        m_response.push_back(std::make_pair(response, std::string(message, response.info.msg_length)));
   }
}

void GetBroadcastMsg::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.src_node);
    ptree.put(TAG::SRC_USER, m_data.info.src_user);
    ptree.put(TAG::BLOCK, m_data.info.block);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}
