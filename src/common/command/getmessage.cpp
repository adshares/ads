#include "getmessage.h"

#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/hash.h"
#include "message.hpp"
#include "command/factory.h"

GetMessage::GetMessage()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

GetMessage::GetMessage(uint16_t abank, uint32_t auser, uint32_t block, uint16_t dstNode, uint32_t msgId, uint32_t time)
    : m_data(abank, auser, block, dstNode, msgId, time) {
    m_responseError = ErrorCodes::Code::eNone;
}

unsigned char* GetMessage::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetMessage::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void GetMessage::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetMessage::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int GetMessage::getDataSize() {
    return sizeof(m_data.info);
}

int GetMessage::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* GetMessage::getSignature() {
    return m_data.sign;
}

int GetMessage::getSignatureSize() {
    return sizeof(m_data.sign);
}

int GetMessage::getType() {
    return TXSTYPE_MSG;
}

void GetMessage::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetMessage::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return (ed25519_sign_open(getData(), getDataSize(), pk, getSignature()) == 0);
}

void GetMessage::saveResponse(settings& /*sts*/) {
}

uint32_t GetMessage::getUserId() {
    return m_data.info.src_user;
}

uint32_t GetMessage::getBankId() {
    return m_data.info.src_node;
}

uint32_t GetMessage::getTime() {
    return m_data.info.ttime;
}

int64_t GetMessage::getFee() {
    return 0;
}

int64_t GetMessage::getDeduct() {
    return 0;
}

user_t& GetMessage::getUserInfo() {
    return m_response.usera;
}

bool GetMessage::send(INetworkClient& netClient) {
    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("GetMessage sending error\n");
        return false;
    }

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("GetMessage reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    // msgTypeLength contains type (1B) and length (3B)
    uint32_t msgTypeLength = 0;
    if (!netClient.readData((int32_t*)&msgTypeLength, sizeof(msgTypeLength))) {
        ELOG("GetMessage reading length error\n");
        return false;
    }
    uint32_t msgLength = msgTypeLength >> 8;
    if (!msgTypeLength || msgLength < message::data_offset) {
        m_responseError = ErrorCodes::Code::eBadLength;
        return true;
    }
    uint8_t msgType = msgTypeLength & 0xFF;
    if (msgType != MSGTYPE_MSG) {
        m_responseError = ErrorCodes::Code::eIncorrectType;
        return true;
    }

    char buffer[msgLength];
    memcpy(buffer, &msgTypeLength, 4);
    if (!netClient.readData(buffer + 4, msgLength - 4)) {
        ELOG("GetMessage reading error\n");
        return false;
    }

    m_responseMsg.reset(new message(msgLength, (uint8_t*)buffer));

    return true;
}

uint32_t GetMessage::getBlockTime() {
    return m_data.info.block;
}

uint16_t GetMessage::getDestNode() {
    return m_data.info.dst_node;
}

uint32_t GetMessage::getMsgId() {
    return m_data.info.node_msgid;
}

std::string GetMessage::toString(bool /*pretty*/) {
    return "";
}

void GetMessage::toJson(boost::property_tree::ptree& ptree) {
    if (m_responseError) {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    } else {
        m_responseMsg->read_head();
        ptree.put("node", m_responseMsg->svid);
        ptree.put("node_msid", m_responseMsg->msid);
        ptree.put("time", m_responseMsg->now);
        ptree.put("length", m_responseMsg->len);
        if(!m_responseMsg->hash_tree_fast(m_responseMsg->sigh,m_responseMsg->data,m_responseMsg->len,m_responseMsg->svid,m_responseMsg->msid)) {
            return;
        }
        uint8_t* data_ptr = m_responseMsg->data+message::data_offset;
        uint8_t* data_end = m_responseMsg->data+m_responseMsg->len;
        uint32_t mpos = 1;
        boost::property_tree::ptree transactions;
        std::unique_ptr<IBlockCommand> command;
        while (data_ptr < data_end) {
            command = command::factory::makeCommand(*data_ptr);
            if (command) {
                boost::property_tree::ptree txn;
                command->setData((char*)data_ptr);
                int size = command->getDataSize()  + command->getAdditionalDataSize() + command->getSignatureSize();

                txn.put("id", Helper::print_msg_id(m_responseMsg->svid, m_responseMsg->msid, mpos++));
                command->txnToJson(txn);
                txn.put("size", size);

                data_ptr += size;
                transactions.push_back(std::make_pair("", txn));
            } else {
                m_responseError = ErrorCodes::Code::eIncorrectTransaction;
                ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
                return;
            }
        }
        ptree.add_child("transactions", transactions);
    }
}

void GetMessage::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.src_node);
    ptree.put(TAG::SRC_USER, m_data.info.src_user);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::BLOCK, m_data.info.block);
    ptree.put(TAG::DST_NODE, m_data.info.dst_node);
    ptree.put(TAG::NODE_MSGID, m_data.info.node_msgid);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}
