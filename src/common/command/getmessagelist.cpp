#include "getmessagelist.h"

#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/hash.h"
#include "hash.hpp"
#include "parser/msglistparser.h"

GetMessageList::GetMessageList()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

GetMessageList::GetMessageList(uint16_t abank, uint32_t auser, uint32_t block, uint32_t time)
    : m_data(abank, auser, block, time) {
    m_responseError = ErrorCodes::Code::eNone;
}

unsigned char* GetMessageList::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetMessageList::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void GetMessageList::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetMessageList::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int GetMessageList::getDataSize() {
    return sizeof(m_data.info);
}

int GetMessageList::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* GetMessageList::getSignature() {
    return m_data.sign;
}

int GetMessageList::getSignatureSize() {
    return sizeof(m_data.sign);
}

int GetMessageList::getType() {
    return TXSTYPE_MGS;
}

CommandType GetMessageList::getCommandType() {
    return CommandType::eReadingOnly;
}

void GetMessageList::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetMessageList::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return (ed25519_sign_open(getData(), getDataSize(), pk, getSignature()) == 0);
}

void GetMessageList::saveResponse(settings& /*sts*/) {
}

uint32_t GetMessageList::getUserId() {
    return m_data.info.src_user;
}

uint32_t GetMessageList::getBankId() {
    return m_data.info.src_node;
}

uint32_t GetMessageList::getTime() {
    return m_data.info.ttime;
}

int64_t GetMessageList::getFee() {
    return 0;
}

int64_t GetMessageList::getDeduct() {
    return 0;
}

uint32_t GetMessageList::getUserMessageId() {
    return 0;
}

bool GetMessageList::send(INetworkClient& netClient) {
    if(!sendData(netClient)) {
        return false;
    }

    readDataSize(netClient);

    if(!readResponseError(netClient)) {
        ELOG("GetMessageList reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    uint32_t no_of_msg = 0;
    if (!netClient.readData((int32_t*)&no_of_msg, sizeof(no_of_msg))) {
        ELOG("GetMessageList reading no of messages error\n");
        return false;
    }

    if (!netClient.readData((uint8_t*)&m_responseTxnHash, sizeof(m_responseTxnHash))) {
        ELOG("GetMessageList reading txn has error\n");
        return false;
    }

    for (unsigned int i=0; i<no_of_msg; ++i) {
        MessageRecord record;
        if (!netClient.readData((char*)&record, sizeof(record))){
            ELOG("GetMessageList reading %d record error\n", i);
            return false;
        }
        m_responseMessageList.push_back(record);
    }

    return true;
}

uint32_t GetMessageList::getBlockTime() {
    return m_data.info.block;
}

ErrorCodes::Code GetMessageList::prepareResponse() {
    Parser::MsglistParser msglistParser(m_data.info.block);
    if (!msglistParser.load()) {
        return ErrorCodes::Code::eNoMessageListFile;
    } else {
        std::copy(msglistParser.getHeaderHash(),
                  msglistParser.getHeaderHash()+sizeof(m_responseTxnHash),
                  m_responseTxnHash);
        m_responseMessageList = msglistParser.getMessageList();
    }
    return ErrorCodes::Code::eNone;
}

std::string GetMessageList::toString(bool /*pretty*/) {
    return "";
}

void GetMessageList::toJson(boost::property_tree::ptree& ptree) {
    if (m_responseError) {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
        ptree.put(ERROR_CODE_TAG, m_responseError);
        ptree.put(ERROR_INFO_TAG, m_responseInfo);
    } else {
        char blockhex[9];
        blockhex[8]='\0';
        sprintf(blockhex,"%08X", m_data.info.block);
        ptree.put("block_time_hex", blockhex);
        ptree.put("block_time", m_data.info.block);

        char hash[65];
        hash[64]='\0';
        ed25519_key2text(hash, m_responseTxnHash,32);
        ptree.put("msghash",hash);
        ptree.put("message_count",m_responseMessageList.size());
        hashtree htree;
        boost::property_tree::ptree messages;
        for (auto &it : m_responseMessageList) {
            boost::property_tree::ptree msghash;
            ed25519_key2text(hash, it.hash,32);
            msghash.put_value(Helper::print_msg_pack_id(it.node_id, it.node_msid));
            htree.update(it.hash);
            messages.push_back(std::make_pair("",msghash));
        }
        uint8_t fullHash[SHA256_DIGEST_LENGTH];
        htree.finish(fullHash);


        if(memcmp(m_responseTxnHash, fullHash, SHA256_DIGEST_LENGTH)) {
            ed25519_key2text(hash, fullHash, SHA256_DIGEST_LENGTH);
            ptree.put("msghash_calculated",hash);
            ptree.put("confirmed","no");
        } else {
            ptree.put("confirmed","yes");
        }
        if(m_responseMessageList.size() > 0) {
            ptree.add_child("messages",messages);
        }
    }
}

void GetMessageList::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.src_node);
    ptree.put(TAG::SRC_USER, m_data.info.src_user);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::BLOCK, m_data.info.block);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

std::string GetMessageList::usageHelperToString() {
    std::stringstream ss{};
    ss << "Usage: " << "{\"run\":\"get_message_list\",[\"block\":<block time as hex>]}" << "\n";
    ss << "Example: " << "{\"run\":\"get_message_list\",\"block\":\"5B052CE0\"}" << "\n";
    return ss.str();
}
