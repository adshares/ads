#include "getmessage.h"

#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/hash.h"
#include "message.hpp"

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

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("GetMessage reading header error\n");
        return false;
    }

    uint32_t msglength = 0;
    if (!netClient.readData((int32_t*)&msglength, sizeof(msglength))) {
        ELOG("GetMessage reading length error\n");
        return false;
    }

//    char buffer[msglength];
//    if (!netClient.readData(buffer, sizeof(buffer))) {
//        ELOG("GetMessage reading error\n");
//        return false;
//    }
//    GetMessageResponse m_response;

//    message_ptr msg(new message(msglength, (uint8_t*)buffer));
//    msg->read_head();
//    m_response.svid = msg->svid;
//    m_response.msgid = msg->msid;
//    m_response.time = msg->now;
//    m_response.length = msg->len;
//    std::copy(msg->sigh, msg->sigh+SHA256_DIGEST_LENGTH, m_response.signature);
//    m_response.hash_tree_fast = msg->hash_tree_fast(msg->sigh,msg->data,msg->len,msg->svid,msg->msid);

//    buffer += msg->data + message::data_offset;
//    uint8_t* end  = msg->data + msg->len;
//    if ((uint8_t*)buffer > end) {
//        m_responseError = ErrorCodes::Code::eInvalidMessageFile;
//        return true;
//    }

    ///TODO: finish it in future when all commands done

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
    }
}

