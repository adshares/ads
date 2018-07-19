#include "getaccount.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"

GetAccount::GetAccount()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

GetAccount::GetAccount(uint16_t abank, uint32_t auser, uint16_t bbank, uint32_t buser,
                       uint32_t time)
    : m_data( abank, auser, bbank, buser, time) {
    m_responseError = ErrorCodes::Code::eNone;
}

unsigned char* GetAccount::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetAccount::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void GetAccount::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetAccount::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int GetAccount::getDataSize() {
    return sizeof(m_data.info);
}

int GetAccount::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* GetAccount::getSignature() {
    return m_data.sign;
}

int GetAccount::getSignatureSize() {
    return sizeof(m_data.sign);
}

int GetAccount::getType() {
    return TXSTYPE_INF;
}

CommandType GetAccount::getCommandType() {
    return CommandType::eReadingOnly;
}

uint32_t GetAccount::getUserMessageId() {
    return 0;
}

void GetAccount::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetAccount::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return( ed25519_sign_open( getData() , getDataSize() , pk, getSignature() ) == 0);
}

void GetAccount::saveResponse(settings& sts)
{
    sts.msid = m_response.usera.msid;
    std::copy(m_response.usera.hash, m_response.usera.hash + SHA256_DIGEST_LENGTH, sts.ha.data());
}

uint32_t GetAccount::getUserId() {
    return m_data.info.auser;
}

uint32_t GetAccount::getBankId() {
    return m_data.info.abank;
}

uint32_t GetAccount::getTime() {
    return m_data.info.ttime;
}

int64_t GetAccount::getFee() {
    return 0;
}

int64_t GetAccount::getDeduct() {
    return 0;
}

uint16_t GetAccount::getDestNode(){
    return m_data.info.bbank;
}

uint32_t GetAccount::getDestUser(){
    return m_data.info.buser;
}

bool GetAccount::send(INetworkClient& netClient) {
    if (!netClient.sendData(getData(), getDataSize() + getSignatureSize() )) {
        ELOG("GetAccount sending error\n");
        return false;
    }

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("GetAccount reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    if (!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("GetAccount ERROR reading global info\n");
        return false;
    }

    return true;
}

std::string GetAccount::toString(bool /*pretty*/) {
    return "";
}

void GetAccount::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        print_user(m_response.usera, ptree, true, m_data.info.bbank, m_data.info.buser);
        print_user(m_response.globalusera, ptree, false, m_data.info.bbank, m_data.info.buser);
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void GetAccount::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.abank);
    ptree.put(TAG::SRC_USER, m_data.info.auser);
    ptree.put(TAG::DST_NODE, m_data.info.bbank);
    ptree.put(TAG::DST_USER, m_data.info.buser);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

