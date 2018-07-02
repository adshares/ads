#include "transactionaccepted.h"

#include "abstraction/interfaces.h"
#include "helper/json.h"

TransactionAccepted::TransactionAccepted()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

TransactionAccepted::TransactionAccepted(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t ttime, uint16_t bbank, uint32_t buser, uint8_t public_key[32])
    : m_data(abank, auser, amsid, ttime, bbank, buser, public_key) {
    m_responseError = ErrorCodes::Code::eNone;
}

unsigned char* TransactionAccepted::getData() {
    return reinterpret_cast<unsigned char*>(&m_data);
}

unsigned char* TransactionAccepted::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void TransactionAccepted::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void TransactionAccepted::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int TransactionAccepted::getDataSize() {
    return sizeof(m_data);
}

int TransactionAccepted::getResponseSize() {
    return 0;
}

unsigned char* TransactionAccepted::getSignature() {
    return (unsigned char*)"";
}

int TransactionAccepted::getSignatureSize() {
    return 0;
}

int TransactionAccepted::getType() {
    return TXSTYPE_UOK;
}

CommandType TransactionAccepted::getCommandType() {
    return CommandType::eReadingOnly;
}

void TransactionAccepted::sign(const uint8_t* /*hash*/, const uint8_t* /*sk*/, const uint8_t* /*pk*/) {
    // no signature
}

bool TransactionAccepted::checkSignature(const uint8_t* /*hash*/, const uint8_t* /*pk*/) {
    // no signature
    return true;
}

void TransactionAccepted::saveResponse(settings& /*sts*/) {
}

uint32_t TransactionAccepted::getUserId() {
    return 0;
}

uint32_t TransactionAccepted::getBankId() {
    return 0;
}

uint32_t TransactionAccepted::getTime() {
    return 0;
}

int64_t TransactionAccepted::getFee() {
    return 0;
}

int64_t TransactionAccepted::getDeduct() {
    return 0;
}

uint32_t TransactionAccepted::getUserMessageId() {
    return 0;
}

bool TransactionAccepted::send(INetworkClient& /*netClient*/) {
    return true;
}

std::string TransactionAccepted::toString(bool /*pretty*/) {
    return "";
}

void TransactionAccepted::toJson(boost::property_tree::ptree& /*ptree*/) {
}

void TransactionAccepted::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.ttype));
    ptree.put(TAG::SRC_NODE, m_data.abank);
    ptree.put(TAG::SRC_USER, m_data.auser);
    ptree.put(TAG::MSGID, m_data.amsid);
    ptree.put(TAG::TIME, m_data.ttime);
    ptree.put(TAG::DST_NODE, m_data.bbank);
    ptree.put(TAG::DST_USER, m_data.buser);
    ptree.put(TAG::PKEY, m_data.publicKey);
}
