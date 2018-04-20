#include "changenodekey.h"
#include <iostream>
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"

ChangeNodeKey::ChangeNodeKey()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

ChangeNodeKey::ChangeNodeKey(uint16_t srcBank, uint32_t srcUser, uint32_t msid, uint16_t dstNode, uint32_t time, uint8_t pubkey[32])
    : m_data(srcBank, srcUser, msid, time, dstNode, pubkey) {
    m_responseError = ErrorCodes::Code::eNone;
}

unsigned char* ChangeNodeKey::getData() {
    return reinterpret_cast<unsigned char*>(&m_data);
}

unsigned char* ChangeNodeKey::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void ChangeNodeKey::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void ChangeNodeKey::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int ChangeNodeKey::getDataSize() {
    return sizeof(m_data.info);
}

int ChangeNodeKey::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* ChangeNodeKey::getSignature() {
    return m_data.sign;
}

int ChangeNodeKey::getSignatureSize() {
    return sizeof(m_data.sign);
}

int ChangeNodeKey::getType() {
    return TXSTYPE_BKY;
}

void ChangeNodeKey::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());

}

bool ChangeNodeKey::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return( ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), pk, getSignature() ) == 0);
}

uint32_t ChangeNodeKey::getUserId() {
    return m_data.info.src_user;
}

uint32_t ChangeNodeKey::getBankId() {
    return m_data.info.src_node;
}

uint32_t ChangeNodeKey::getTime() {
    return m_data.info.ttime;
}

int64_t ChangeNodeKey::getFee() {
    return TXS_BKY_FEE;
}

int64_t ChangeNodeKey::getDeduct() {
    return 0;
}

user_t& ChangeNodeKey::getUserInfo() {
    return m_response.usera;
}

bool ChangeNodeKey::send(INetworkClient& netClient)
{
    if(! netClient.sendData(getData(), sizeof(m_data) )) {
        ELOG("ChangeNodeKey sending error\n");
        return false;
    }

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("ChangeNodeKey reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("ChangeNodeKey ERROR reading global info\n");
        return false;
    }

    return true;
}

void ChangeNodeKey::saveResponse(settings& sts) {
    if (!std::equal(sts.pk, sts.pk + SHA256_DIGEST_LENGTH, m_response.usera.pkey)) {
        m_responseError = ErrorCodes::Code::ePkeyDiffers;
    }

    sts.msid = m_response.usera.msid;
    std::copy(m_response.usera.hash, m_response.usera.hash + SHA256_DIGEST_LENGTH, sts.ha.data());
}

uint32_t ChangeNodeKey::getDestBankId() {
    return m_data.info.dst_node;
}

uint8_t* ChangeNodeKey::getKey() {
    return m_data.info.node_new_key;
}

void ChangeNodeKey::setOldPublicKey(uint8_t *data) {
    std::copy(data, data + 32, m_data.info.old_public_key);
}

uint8_t* ChangeNodeKey::getOldPublicKey() {
    return m_data.info.old_public_key;
}

uint32_t ChangeNodeKey::getUserMessageId() {
    return m_data.info.msg_id;
}

std::string ChangeNodeKey::toString(bool /*pretty*/) {
    return "";
}

void ChangeNodeKey::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        ptree.put("result", "Node key changed");
    } else {
        if (m_responseError == ErrorCodes::Code::ePkeyDiffers) {
            std::stringstream tx_user_hashin{};
            Helper::ed25519_key2text(tx_user_hashin, m_response.usera.pkey, SHA256_DIGEST_LENGTH);
            ptree.put("tx.account_public_key_new", tx_user_hashin.str());
        }
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}
