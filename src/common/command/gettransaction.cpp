#include "gettransaction.h"

#include <boost/filesystem.hpp>

#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/hash.h"
#include "helper/servers.h"
#include "hash.hpp"
#include "message.hpp"
#include "factory.h"

GetTransaction::GetTransaction()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

GetTransaction::GetTransaction(uint16_t src_node, uint32_t src_user, uint16_t dst_node, uint32_t node_msgid, uint32_t position, uint32_t time)
    : m_data(src_node, src_user, time, dst_node, node_msgid, position) {
    m_responseError = ErrorCodes::Code::eNone;
}

unsigned char* GetTransaction::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetTransaction::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void GetTransaction::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetTransaction::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int GetTransaction::getDataSize() {
    return sizeof(m_data.info);
}

int GetTransaction::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* GetTransaction::getSignature() {
    return m_data.sign;
}

int GetTransaction::getSignatureSize() {
    return sizeof(m_data.sign);
}

int GetTransaction::getType() {
    return TXSTYPE_TXS;
}

CommandType GetTransaction::getCommandType() {
    return CommandType::eReadingOnly;
}

void GetTransaction::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetTransaction::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return (ed25519_sign_open(getData(), getDataSize(), pk, getSignature()) == 0);
}

void GetTransaction::saveResponse(settings& /*sts*/) {
}

uint32_t GetTransaction::getUserId() {
    return m_data.info.src_user;
}

uint32_t GetTransaction::getBankId() {
    return m_data.info.src_node;
}

uint32_t GetTransaction::getTime() {
    return m_data.info.ttime;
}

int64_t GetTransaction::getFee() {
    return 0;
}

int64_t GetTransaction::getDeduct() {
    return 0;
}

uint32_t GetTransaction::getUserMessageId() {
    return 0;
}

bool GetTransaction::send(INetworkClient& netClient) {
    if (loadFromLocal()) {
        return true;
    }

    if(!sendData(netClient)) {
        return false;
    }

    readDataSize(netClient);

    if(!readResponseError(netClient)) {
        ELOG("GetTransaction reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    if (!netClient.readData((char*)&m_responseHeader, sizeof(m_responseHeader))) {
        ELOG("GetTransaction reading header error\n");
        return false;
    }

    uint8_t data[m_responseHeader.len];
    if (!netClient.readData(data, sizeof(data))) {
        ELOG("GetTransaction reading data error\n");
        return false;
    }

    for (unsigned int i=0; i < m_responseHeader.hnum; ++i) {
        hash_s hash;
        if (!netClient.readData((char*)&hash, sizeof(hash))) {
            ELOG("GetTransaction reading hashes error\n");
            return false;
        }
        m_responseHashes.push_back(hash);
    }

    if (!checkMessageHash((char*)data) || !saveToFile((char*)data)) {
        return true;
    }

    m_responseTxn = command::factory::makeCommand(*data);
    if (m_responseTxn) {
        m_responseTxn->setData((char*)data);
    } else {
        m_responseError = ErrorCodes::Code::eIncorrectTransaction;
    }

    return true;
}

bool GetTransaction::checkMessageHash(char *data) {
    uint16_t svid= m_data.info.dst_node;
    uint32_t msid= m_data.info.node_msgid;
    uint16_t tnum= m_data.info.position;
    if (!m_responseHeader.path) {
        m_responseError = ErrorCodes::Code::eGotEmptyBlock;
        ELOG("ERROR, got empty block for txid %04X:%08X:%04X\n",svid,msid,tnum);
        return false;
    }
    if (m_responseHeader.node != svid
               || m_responseHeader.msid != msid
               || m_responseHeader.tnum != tnum) {
        m_responseError = ErrorCodes::Code::eIncorrectTransaction;
        ELOG("ERROR, got wrong transaction %04X%08X%04X for txid %04X:%08X:%04X\n",
             m_responseHeader.node, m_responseHeader.msid, m_responseHeader.tnum, svid, msid, tnum);
        return false;
    }
    Helper::Servers block;
    block.setNow(m_responseHeader.path);
    if (!block.loadNowhash()) {
        m_responseError = ErrorCodes::Code::eFailedToLoadHash;
        return false;
    }

    hash_t nhash;
    uint16_t* hashsvid=(uint16_t*)nhash;
    uint32_t* hashmsid=(uint32_t*)(&nhash[4]);
    uint16_t* hashtnum=(uint16_t*)(&nhash[8]);
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data, m_responseHeader.len);
    SHA256_Final(nhash,&sha256);
    *hashsvid^=svid;
    *hashmsid^=msid;
    *hashtnum^=tnum;

    if(memcmp(nhash, &m_responseHashes[0], 32)) {
        ELOG("ERROR, failed to confirm first hash for txid %04X:%08X:%04X\n",svid,msid,tnum);
        m_responseError = ErrorCodes::Code::eHashMismatch;
        return false;
    }

    hashtree tree;
    tree.hashpathrun(nhash, m_responseHashes);
    if(memcmp(nhash, block.getNowHash(), 32)) {
        ELOG("ERROR, failed to confirm nowhash for txid %04X:%08X:%04X\n",svid,msid,tnum);
        m_responseError = ErrorCodes::Code::eHashMismatch;
        return false;
    }
    return true;
}

bool GetTransaction::saveToFile(char *data) {
    using namespace boost::filesystem;
    uint8_t dir[6];
    memcpy(dir,&m_data.info.dst_node, sizeof(m_data.info.dst_node));
    memcpy(dir+2,&m_data.info.node_msgid, sizeof(m_data.info.node_msgid));
    char filename[128];
    sprintf(filename,"txs/%02X/%02X/%02X/%02X/%02X/%02X",dir[1],dir[0],dir[5],dir[4],dir[3],dir[2]);
    create_directories(filename);
    if (is_directory(filename) == false) {
        m_responseError = ErrorCodes::Code::eCantCreateDirectory;
        return false;
    }
    permissions(filename, owner_all | group_read | group_exe | others_read | others_exe);
    sprintf(filename, "%s/%04X.txs", filename, m_data.info.position);
    std::ofstream file(filename, std::ofstream::out | std::ofstream::binary);
    if (file.is_open()) {
        file.write((char*)&m_responseHeader, sizeof(m_responseHeader));
        file.write(data, m_responseHeader.len);
        for (unsigned int i=0; i<m_responseHashes.size(); ++i) {
            file.write((char*)&m_responseHashes[i], sizeof(hash_s));
        }
        file.close();
        permissions(filename, owner_all | group_read | group_exe | others_read | others_exe);
        return true;
    }
    m_responseError = ErrorCodes::Code::eCantOpenFile;
    return false;
}


uint16_t GetTransaction::getDestinationNode() {
    return m_data.info.dst_node;
}

uint32_t GetTransaction::getNodeMsgId() {
    return m_data.info.node_msgid;
}

uint16_t GetTransaction::getPosition() {
    return m_data.info.position;
}

bool GetTransaction::loadFromLocal() {
    uint8_t dir[6];
    memcpy(dir,&m_data.info.dst_node, sizeof(m_data.info.dst_node));
    memcpy(dir+2,&m_data.info.node_msgid, sizeof(m_data.info.node_msgid));
    char filename[128];
    sprintf(filename,"txs/%02X/%02X/%02X/%02X/%02X/%02X/%04X.txs",dir[1],dir[0],dir[5],dir[4],dir[3],dir[2],m_data.info.position);
    std::ifstream file(filename, std::ifstream::in | std::ifstream::binary);
    if (file.is_open()) {
        file.read((char*)&m_responseHeader, sizeof(m_responseHeader));
        uint8_t data[m_responseHeader.len];
        file.read((char*)data, sizeof(data));
        m_responseTxn = command::factory::makeCommand(*data);
        if (m_responseTxn) {
            m_responseTxn->setData((char*)data);
            for (unsigned int i=0; i< m_responseHeader.hnum; ++i) {
                hash_s hash;
                file.read((char*)&hash, sizeof(hash_s));
                m_responseHashes.push_back(hash);
            }
        } else {
            m_responseError = ErrorCodes::Code::eIncorrectTransaction;
        }
        file.close();
        return true;
    }
    return false;
}

std::string GetTransaction::toString(bool /*pretty*/) {
    return "";
}

void GetTransaction::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError && !m_responseTxn) {
        m_responseError = ErrorCodes::Code::eIncorrectTransaction;
    }
    if (m_responseError) {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
        ptree.put(ERROR_CODE_TAG, m_responseError);
        ptree.put(ERROR_INFO_TAG, m_responseInfo);
    } else {
        char tx_id[64];
        sprintf(tx_id,"%04X:%08X:%04X",m_responseHeader.node,m_responseHeader.msid,m_responseHeader.tnum);
        ptree.put("network_tx.id",&tx_id[0]);
        ptree.put("network_tx.block_time",m_responseHeader.path);

        char blockhex[9];
        blockhex[8]='\0';
        sprintf(blockhex,"%08X", m_responseHeader.path);
        ptree.put("network_tx.block_id", blockhex);

        ptree.put("network_tx.node",m_responseHeader.node);
        ptree.put("network_tx.node_msid",m_responseHeader.msid);
        ptree.put("network_tx.node_mpos",m_responseHeader.tnum);
        ptree.put("network_tx.size",m_responseHeader.len);
        ptree.put("network_tx.hashpath_size",m_responseHeader.hnum);
        std::stringstream full_data, tx_data;
        full_data.write((char*)m_responseTxn->getData(), m_responseTxn->getDataSize());
        full_data.write((char*)m_responseTxn->getAdditionalData(), m_responseTxn->getAdditionalDataSize());
        full_data.write((char*)m_responseTxn->getSignature(), m_responseTxn->getSignatureSize());
        Helper::ed25519_key2text(tx_data, (uint8_t*)full_data.str().c_str(), m_responseTxn->getDataSize() + m_responseTxn->getAdditionalDataSize() + m_responseTxn->getSignatureSize());
        ptree.put("network_tx.data", tx_data.str());
        boost::property_tree::ptree hashpath;
        for(int i=0; i<m_responseHeader.hnum; i++) {
            char hashtext[65];
            hashtext[64]='\0';
            ed25519_key2text(hashtext, (uint8_t*)&m_responseHashes[i], sizeof(hash_t));
            boost::property_tree::ptree hashstep;
            hashstep.put("",&hashtext[0]);
            hashpath.push_back(std::make_pair("",hashstep));
        }
        ptree.add_child("network_tx.hashpath",hashpath);
        boost::property_tree::ptree pt;
        m_responseTxn->txnToJson(pt);
        ptree.add_child("txn", pt);
    }
}

void GetTransaction::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.src_node);
    ptree.put(TAG::SRC_USER, m_data.info.src_user);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::DST_NODE, m_data.info.dst_node);
    ptree.put(TAG::NODE_MSGID, m_data.info.node_msgid);
    ptree.put(TAG::POSITION, m_data.info.position);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

std::string GetTransaction::usageHelperToString() {
    std::stringstream ss{};
    ss << "Usage: " << "{\"run\":\"get_transaction\",\"txid\":<transaction_id>}" << "\n";
    ss << "Example: " << "{\"run\":\"get_transaction\",\"txid\":\"0001:00000002:0001\"}" << "\n";
    return ss.str();
}
