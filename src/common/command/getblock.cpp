#include "getblock.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/ascii.h"
#include "helper/hlog.h"
#include "helper/blocks.h"

GetBlock::GetBlock()
    : m_data{}, m_hlog{} {
}

GetBlock::GetBlock(uint16_t src_bank, uint32_t src_user, uint32_t block, uint32_t time)
    : m_data(src_bank, src_user, block, time), m_hlog{} {
    if (!block) {
        m_data.info.block = time-(time%BLOCKSEC)-BLOCKSEC;
    }
}

GetBlock::~GetBlock() {

}

unsigned char* GetBlock::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetBlock::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_responseHeader);
}

void GetBlock::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetBlock::setResponse(char* response) {
    m_responseHeader = *reinterpret_cast<decltype(m_responseHeader)*>(response);
}

int GetBlock::getDataSize() {
    return sizeof(m_data.info);
}

int GetBlock::getResponseSize() {
    return sizeof(m_responseHeader);
}

unsigned char* GetBlock::getSignature() {
    return m_data.sign;
}

int GetBlock::getSignatureSize() {
    return sizeof(m_data.sign);
}

int GetBlock::getType() {
    return TXSTYPE_NDS;
}

CommandType GetBlock::getCommandType() {
    return CommandType::eReadingOnly;
}

void GetBlock::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetBlock::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return (ed25519_sign_open(getData(), getDataSize(), pk, getSignature()) == 0);
}

void GetBlock::saveResponse(settings& /*sts*/) {
}

uint32_t GetBlock::getUserId() {
    return m_data.info.src_user;
}

uint32_t GetBlock::getBankId() {
    return m_data.info.src_node;
}

uint32_t GetBlock::getTime() {
    return m_data.info.ttime;
}

int64_t GetBlock::getFee() {
    return 0;
}

int64_t GetBlock::getDeduct() {
    return 0;
}

bool GetBlock::send(INetworkClient& netClient) {
    sendDataSize(netClient);

    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("GetBlock sending error\n");
        return false;
    }

    readDataSize(netClient);

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("GetBlock reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("GetBlock ERROR reading response header info\n");
        return false;
    }

    for (unsigned int i=0; i<m_responseHeader.nodesCount; ++i) {
        Helper::ServersNode node;
        if (!netClient.readData((char*)&node, sizeof(node))) {
            ELOG("GetBlock ERROR reading nodes\n");
            return false;
        }
        m_responseNodes.push_back(node);
    }

    // read Hlog
    uint32_t hlogSize;
    if (!netClient.readData((int32_t*)&hlogSize, sizeof(uint32_t))) {
        ELOG("GetBlock Error reading Hlog size\n");
        return false;
    }

    if (hlogSize > 0) {
        char *hlogBuffer = new char[hlogSize];
        if (!netClient.readData(hlogBuffer, hlogSize)) {
            ELOG("GetBlock Error reading Hlog\n");
        }
        m_hlog = Helper::Hlog(hlogBuffer, hlogSize);
        delete[] hlogBuffer;
    }

    return true;
}

uint32_t GetBlock::getBlockId() {
    return m_data.info.block;
}

uint32_t GetBlock::getUserMessageId() {
    return 0;
}

ErrorCodes::Code GetBlock::prepareResponse() {
    uint32_t path = m_data.info.block;
    char filename[64];
    Helper::FileName::getName(filename, path, "servers.srv");
    Helper::Servers servers(filename);
    servers.load();

    if (servers.getNodesCount() > 0) {
        m_responseHeader = servers.getHeader();
        m_responseNodes = servers.getNodes();
    } else {
        return ErrorCodes::Code::eGetBlockInfoUnavailable;
    }
    return ErrorCodes::Code::eNone;
}

std::string GetBlock::toString(bool /*pretty*/) {
    return "";
}

void GetBlock::printSingleNode(boost::property_tree::ptree& tree, int nodeId, ServersNode& serverNode) {
    boost::property_tree::ptree node;

    char nodehex[5];
    nodehex[4]='\0';
    sprintf(nodehex,"%04X", nodeId);
    node.put("id", nodehex);

    serverNode.toJson(node);

    tree.push_back(std::make_pair("",node));
}

void GetBlock::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        boost::property_tree::ptree blocktree;
        char blockhex[9];
        blockhex[8]='\0';

        sprintf(blockhex,"%08X", m_data.info.block);
        ptree.put("block_time_hex", blockhex);
        blocktree.put("id", blockhex);
        ptree.put("block_time", m_data.info.block);
        sprintf(blockhex,"%08X", m_data.info.block-BLOCKSEC);
        ptree.put("block_prev",blockhex);
        sprintf(blockhex,"%08X",m_data.info.block+BLOCKSEC);
        ptree.put("block_next",blockhex);

        m_responseHeader.toJson(blocktree);
        boost::property_tree::ptree nodes;
        for (unsigned int i=0; i<m_responseNodes.size(); ++i) {
            printSingleNode(nodes, i, m_responseNodes[i]);
        }
        blocktree.add_child("nodes", nodes);
        ptree.add_child("block", blocktree);

        if (m_hlog.total > 0) {
            m_hlog.printJson(ptree);
        }
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void GetBlock::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.src_node);
    ptree.put(TAG::SRC_USER, m_data.info.src_user);
    ptree.put(TAG::BLOCK, m_data.info.block);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

std::string GetBlock::usageHelperToString() {
    std::stringstream ss{};
    ss << "Usage: " << "{\"run\":\"get_block\",[\"block\":<block_time as hex>]}" << "\n";
    ss << "Example: " << "{\"run\":\"get_block\",\"block\":\"5B217880\"}" << "\n";
    return ss.str();
}
