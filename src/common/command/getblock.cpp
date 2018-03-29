#include "getblock.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/ascii.h"
#include "helper/hlog.h"

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

user_t& GetBlock::getUserInfo() {
    // in this case there is a multiple user info fields
    return *(user_t*)nullptr;
}

bool GetBlock::send(INetworkClient& netClient) {
    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("GetBlock sending error\n");
        return false;
    }

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

ErrorCodes::Code GetBlock::prepareResponse() {
    uint32_t path = m_data.info.block;
    char filename[64];
    sprintf(filename,"blk/%03X/%05X/servers.srv",path>>20,path&0xFFFFF);

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

void GetBlock::printSingleNode(boost::property_tree::ptree& tree, unsigned int nodeId) {
    boost::property_tree::ptree node;

    char nodehex[5];
    nodehex[4]='\0';
    sprintf(nodehex,"%04X", nodeId);
    node.put("id", nodehex);

    std::vector<ServersNode>::iterator nodeIt = m_responseNodes.begin() + nodeId;

    char hash[65];
    hash[64]='\0';
    Helper::ed25519_key2text(hash, nodeIt->publicKey, 32);
    node.put("public_key", hash);
    Helper::ed25519_key2text(hash, (uint8_t*)&nodeIt->hash[0], 32);
    node.put("hash", hash);
    Helper::ed25519_key2text(hash, nodeIt->messageHash, 32);
    node.put("message_hash", hash);
    node.put("msid", nodeIt->messageId);
    node.put("mtim", nodeIt->messageTime);
    node.put("balance", print_amount(nodeIt->weight));
    node.put("status", nodeIt->status);
    node.put("account_count", nodeIt->accountCount);
    node.put("port", nodeIt->port);

    struct in_addr ip_addr;
    ip_addr.s_addr = nodeIt->ipv4;
    node.put("ipv4", inet_ntoa(ip_addr));

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

        blocktree.put("time", m_responseHeader.ttime);
        blocktree.put("message_count", m_responseHeader.messageCount);

        char hash[65];
        hash[64]='\0';
        Helper::ed25519_key2text(hash, m_responseHeader.oldHash, 32);
        blocktree.put("oldhash", hash);
        Helper::ed25519_key2text(hash, m_responseHeader.minHash, 32);
        blocktree.put("minhash", hash);
        Helper::ed25519_key2text(hash, m_responseHeader.msgHash, 32);
        blocktree.put("msghash", hash);
        Helper::ed25519_key2text(hash, m_responseHeader.nodHash, 32);
        blocktree.put("nodhash", hash);
        Helper::ed25519_key2text(hash, m_responseHeader.vipHash, 32);
        blocktree.put("viphash", hash);
        Helper::ed25519_key2text(hash, m_responseHeader.nowHash, 32);
        blocktree.put("nowhash", hash);
        blocktree.put("vote_yes", m_responseHeader.voteYes);
        blocktree.put("vote_no", m_responseHeader.voteNo);
        blocktree.put("vote_total", m_responseHeader.voteTotal);

        blocktree.put("dividend_balance", Helper::print_amount(m_responseHeader.dividendBalance));
        if(!((m_responseHeader.ttime/BLOCKSEC)%BLOCKDIV)) {
            blocktree.put("dividend_pay","true");
        } else {
            blocktree.put("dividend_pay","false");
        }
        blocktree.put("node_count", m_responseHeader.nodesCount);
        boost::property_tree::ptree nodes;
        for (unsigned int i=0; i<m_responseNodes.size(); ++i) {
            printSingleNode(nodes, i);
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
