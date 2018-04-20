#include "setaccountkey.h"

#include <iostream>
#include <algorithm>

#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/hash.h"

SetAccountKey::SetAccountKey()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

SetAccountKey::SetAccountKey(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t time, uint8_t pubkey[32], uint8_t pubkeysign[64])
    : m_data( abank, auser, amsid, time, pubkey, pubkeysign) {
    m_responseError = ErrorCodes::Code::eNone;
}

bool SetAccountKey::checkPubKeySignaure() {
    return(ed25519_sign_open((uint8_t*)nullptr, 0, m_data.pubkey , m_data.pubkeysign) != -1);
}

unsigned char* SetAccountKey::getData() {
    return reinterpret_cast<unsigned char*>(&m_data);
}

unsigned char* SetAccountKey::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void SetAccountKey::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void SetAccountKey::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int SetAccountKey::getDataSize() {
    return sizeof(m_data.ttype) + sizeof(m_data.abank) + sizeof(m_data.auser) + sizeof(m_data.amsid) + sizeof(m_data.ttime) + sizeof(m_data.pubkey);
}

int SetAccountKey::getResponseSize() {
    return sizeof(user_t);
}

unsigned char* SetAccountKey::getSignature() {
    return m_data.sign;
}

int SetAccountKey::getSignatureSize() {
    return sizeof(m_data.sign);
}

int SetAccountKey::getType() {
    return TXSTYPE_KEY;
}

void SetAccountKey::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());
}

bool SetAccountKey::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return( ed25519_sign_open2( hash , SHA256_DIGEST_LENGTH , getData(), getDataSize(), pk, getSignature() ) == 0);
}

uint32_t SetAccountKey::getUserId() {
    return m_data.auser;
}

uint32_t SetAccountKey::getBankId() {
    return m_data.abank;
}

uint32_t SetAccountKey::getTime() {
    return m_data.ttime;
}

int64_t SetAccountKey::getFee() {
    return TXS_KEY_FEE;
}

int64_t SetAccountKey::getDeduct() {
    return 0;
}

user_t& SetAccountKey::getUserInfo() {
    return m_response.usera;
}

bool SetAccountKey::send(INetworkClient& netClient)
{
    if(! netClient.sendData(getData(), sizeof(m_data) )) {
        ELOG("SetAccountKey sending error\n");
        return false;
    }

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("SetAccountKey reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("SetAccountKey ERROR reading global info\n");
        return false;
    }

    return true;
}

void SetAccountKey::saveResponse(settings& sts)
{
    if (std::equal(sts.pk, sts.pk + SHA256_DIGEST_LENGTH, m_response.usera.pkey)) {
        m_responseError = ErrorCodes::Code::ePkeyNotChanged;
    }

    std::array<uint8_t, SHA256_DIGEST_LENGTH> hashout;
    Helper::create256signhash(getSignature(), getSignatureSize(), sts.ha, hashout);
    if (!std::equal(hashout.begin(), hashout.end(), m_response.usera.hash)) {
        m_responseError = ErrorCodes::Code::eHashMismatch;
    }

    sts.msid = m_response.usera.msid;
    std::copy(m_response.usera.pkey, m_response.usera.pkey + SHA256_DIGEST_LENGTH, sts.pk);        
    std::copy(m_response.usera.hash, m_response.usera.hash + SHA256_DIGEST_LENGTH, sts.ha.data());
}

std::string SetAccountKey::toString(bool /*pretty*/) {
    return "";
}

void SetAccountKey::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        ptree.put("result", "PKEY changed");
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}
