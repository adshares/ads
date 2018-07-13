#include "transactionfailed.h"

#include "abstraction/interfaces.h"
#include "helper/json.h"

TransactionFailed::TransactionFailed()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

TransactionFailed::TransactionFailed(uint8_t messageSize[3])
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
    memcpy(&m_data.message_size, messageSize, 3);
    if (m_data.message_size < 4) {
        m_data.message_size = 4;
    }
}

unsigned char* TransactionFailed::getData() {
    return reinterpret_cast<unsigned char*>(&m_data);
}

unsigned char* TransactionFailed::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void TransactionFailed::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
    memcpy(&m_data.message_size, data + 1, 3);
}

void TransactionFailed::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int TransactionFailed::getDataSize() {
    return m_data.message_size;
}

int TransactionFailed::getResponseSize() {
    return 0;
}

unsigned char* TransactionFailed::getSignature() {
    return (unsigned char*)"";
}

int TransactionFailed::getSignatureSize() {
    return 0;
}

int TransactionFailed::getType() {
    return TXSTYPE_NON;
}

CommandType TransactionFailed::getCommandType() {
    return CommandType::eReadingOnly;
}

void TransactionFailed::sign(const uint8_t* /*hash*/, const uint8_t* /*sk*/, const uint8_t* /*pk*/) {
    // no signature
}

bool TransactionFailed::checkSignature(const uint8_t* /*hash*/, const uint8_t* /*pk*/) {
    // no signature
    return true;
}

void TransactionFailed::saveResponse(settings& /*sts*/) {
}

uint32_t TransactionFailed::getUserId() {
    return 0;
}

uint32_t TransactionFailed::getBankId() {
    return 0;
}

uint32_t TransactionFailed::getTime() {
    return 0;
}

int64_t TransactionFailed::getFee() {
    return 0;
}

int64_t TransactionFailed::getDeduct() {
    return 0;
}

uint32_t TransactionFailed::getUserMessageId() {
    return 0;
}

bool TransactionFailed::send(INetworkClient& /*netClient*/) {
    return true;
}

std::string TransactionFailed::toString(bool /*pretty*/) {
    return "";
}

void TransactionFailed::toJson(boost::property_tree::ptree& /*ptree*/) {
}

void TransactionFailed::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.ttype));
}

