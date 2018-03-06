#include "createnode.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"

CreateNode::CreateNode()
    : m_data{} {
}

CreateNode::CreateNode(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t ttime) 
	: m_data(abank, auser, amsid, ttime) {
}

unsigned char* CreateNode::getData() {
    return reinterpret_cast<unsigned char*>(&m_data);
}

unsigned char* CreateNode::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void CreateNode::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void CreateNode::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int CreateNode::getDataSize() {
    return sizeof(m_data.data);
}

int CreateNode::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* CreateNode::getSignature() {
    return m_data.sign;
}

int CreateNode::getSignatureSize() {
    return sizeof(m_data.sign);
}

int CreateNode::getType() {
    return TXSTYPE_BNK;
}

void CreateNode::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());
}

bool CreateNode::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return( ed25519_sign_open2( hash , SHA256_DIGEST_LENGTH , getData(), getDataSize(), pk, getSignature() ) == 0);
}

void CreateNode::saveResponse(settings& sts) {
    sts.msid = m_response.usera.msid;
    std::copy(m_response.usera.hash, m_response.usera.hash + SHA256_DIGEST_LENGTH, sts.ha.data());
}

uint32_t CreateNode::getUserId() {
    return m_data.data.auser;
}

uint32_t CreateNode::getBankId() {
    return m_data.data.abank;
}

uint32_t CreateNode::getTime() {
    return m_data.data.ttime;
}

int64_t CreateNode::getFee() {
    return TXS_BNK_FEE;
}

int64_t CreateNode::getDeduct() {
    return BANK_MIN_TMASS;
}

user_t& CreateNode::getUserInfo() {
    return m_response.usera;
}

bool CreateNode::send(INetworkClient& netClient) {
    if(! netClient.sendData(getData(), getDataSize() + getSignatureSize() )) {
        return false;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        std::cerr<<"CreateNode ERROR reading global info\n";
        return false;
    }

    return true;
}

std::string CreateNode::toString(bool pretty) {
    return "";
}

boost::property_tree::ptree CreateNode::toJson() {
    return boost::property_tree::ptree();
}
