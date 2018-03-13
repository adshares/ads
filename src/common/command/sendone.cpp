#include "sendone.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"

SendOne::SendOne()
    : m_data{} {
}

SendOne::SendOne(uint16_t abank, uint32_t auser, uint32_t amsid, uint16_t bbank,
                 uint16_t buser, int64_t tmass, uint8_t tinfo[32], uint32_t time)
    : m_data(abank, auser, amsid, bbank, buser, tmass, time, tinfo) {
}

unsigned char* SendOne::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* SendOne::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void SendOne::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void SendOne::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int SendOne::getDataSize() {
    return sizeof(m_data.info);
}

int SendOne::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* SendOne::getSignature() {
    return m_data.sign;
}

int SendOne::getSignatureSize() {
    return sizeof(m_data.sign);
}

int SendOne::getType() {
    return TXSTYPE_PUT;
}

void SendOne::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash,SHA256_DIGEST_LENGTH , getData(), getDataSize(), sk, pk, getSignature());
}

bool SendOne::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return (ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH,getData(),getDataSize(),pk,getSignature()) == 0);

}

void SendOne::saveResponse(settings& sts) {
    sts.msid = m_response.usera.msid;
    std::copy(m_response.usera.hash, m_response.usera.hash + SHA256_DIGEST_LENGTH, sts.ha.data());
}

uint32_t SendOne::getUserId() {
    return m_data.info.auser;
}

uint32_t SendOne::getBankId() {
    return m_data.info.abank;
}

uint32_t SendOne::getTime() {
    return m_data.info.ttime;
}

int64_t SendOne::getFee() {
    int64_t fee=TXS_PUT_FEE(m_data.info.ntmass);
    if(m_data.info.abank!=m_data.info.bbank) {
        fee+=TXS_LNG_FEE(m_data.info.ntmass);
    }
    return fee;
}

int64_t SendOne::getDeduct() {
    return m_data.info.ntmass;
}

user_t& SendOne::getUserInfo() {
    return m_response.usera;
}

bool SendOne::send(INetworkClient& netClient) {
    if(!netClient.sendData(getData(), sizeof(m_data))) {
        return false;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        std::cerr<<"SendOne ERROR reading global info\n";
        return false;
    }

    return true;
}

uint32_t SendOne::getDestBankId() {
    return m_data.info.bbank;
}

uint32_t SendOne::getDestUserId() {
    return m_data.info.buser;
}

uint32_t SendOne::getUserMessageId() {
    return m_data.info.namsid;
}

uint8_t* SendOne::getInfoMsg() {
    return m_data.info.ntinfo;
}

std::string SendOne::toString(bool /*pretty*/) {
    return "";
}

boost::property_tree::ptree SendOne::toJson() {
    return boost::property_tree::ptree();
}
