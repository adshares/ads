#include "getaccount.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"

GetAccount::GetAccount()
    : m_data{} {
}

GetAccount::GetAccount(uint16_t abank, uint32_t auser, uint16_t bbank, uint16_t buser,
                       uint32_t time)
    : m_data( abank, auser, bbank, buser, time) {
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

user_t& GetAccount::getUserInfo() {
    return m_response.usera;
}

bool GetAccount::send(INetworkClient& netClient) {
    if(! netClient.sendData(getData(), getDataSize() + getSignatureSize() )) {
        return false;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        std::cerr<<"GetAccount ERROR reading global info\n";
        return false;
    }

    return true;
}

std::string GetAccount::toString(bool /*pretty*/) {
    return "";
}

void GetAccount::toJson(boost::property_tree::ptree& ptree) {
    print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
    print_user(m_response.globalusera, ptree, false, this->getBankId(), this->getUserId());
}
