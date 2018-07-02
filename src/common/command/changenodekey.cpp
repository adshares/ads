#include "changenodekey.h"
#include <iostream>
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/hash.h"
#include "helper/json.h"

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
    return sizeof(m_data) - getSignatureSize() - sizeof(m_data.old_public_key);
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

CommandType ChangeNodeKey::getCommandType() {
    return CommandType::eModifying;
}

void ChangeNodeKey::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());

}

bool ChangeNodeKey::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return( ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), pk, getSignature() ) == 0);
}

uint32_t ChangeNodeKey::getUserId() {
    return m_data.src_user;
}

uint32_t ChangeNodeKey::getBankId() {
    return m_data.src_node;
}

uint32_t ChangeNodeKey::getTime() {
    return m_data.ttime;
}

unsigned char*  ChangeNodeKey::getBlockMessage()
{
    return reinterpret_cast<unsigned char*>(&m_data);
}

size_t  ChangeNodeKey::getBlockMessageSize()
{
    return sizeof(ChangeNodeKeyData);
}

/*unsigned char*  ChangeNodeKey::getAdditionalData()
{
    return reinterpret_cast<unsigned char*>(&m_data.old_public_key);
}

int ChangeNodeKey::getAdditionalDataSize() {
    return sizeof(m_data.old_public_key);
}*/

int64_t ChangeNodeKey::getFee() {
    return TXS_BKY_FEE;
}

int64_t ChangeNodeKey::getDeduct() {
    return 0;
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

    std::array<uint8_t, SHA256_DIGEST_LENGTH> hashout;
    Helper::create256signhash(getSignature(), getSignatureSize(), sts.ha, hashout);
    if (!std::equal(hashout.begin(), hashout.end(), m_response.usera.hash)) {
        m_responseError = ErrorCodes::Code::eHashMismatch;
    }

    sts.msid = m_response.usera.msid;
    std::copy(m_response.usera.hash, m_response.usera.hash + SHA256_DIGEST_LENGTH, sts.ha.data());
}

uint32_t ChangeNodeKey::getDestBankId() {
    return m_data.dst_node;
}

uint8_t* ChangeNodeKey::getKey() {
    return m_data.node_new_key;
}

void ChangeNodeKey::setOldPublicKey(uint8_t *data) {
    std::copy(data, data + 32, m_data.old_public_key);
}

uint8_t* ChangeNodeKey::getOldPublicKey() {
    return m_data.old_public_key;
}

uint32_t ChangeNodeKey::getUserMessageId() {
    return m_data.msg_id;
}

std::string ChangeNodeKey::toString(bool /*pretty*/) {
    return "";
}

void ChangeNodeKey::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        ptree.put("result", "Node key changed");
        Helper::print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        Helper::print_msgid_info(ptree, m_data.src_node, m_response.msid, m_response.mpos);
    } else {
        if (m_responseError == ErrorCodes::Code::ePkeyDiffers) {
            std::stringstream tx_user_hashin{};
            Helper::ed25519_key2text(tx_user_hashin, m_response.usera.pkey, SHA256_DIGEST_LENGTH);
            ptree.put("tx.account_public_key_new", tx_user_hashin.str());
        }
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void ChangeNodeKey::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.ttype));
    ptree.put(TAG::SRC_NODE, m_data.src_node);
    ptree.put(TAG::SRC_USER, m_data.src_user);
    ptree.put(TAG::MSGID, m_data.msg_id);
    ptree.put(TAG::TIME, m_data.ttime);
    ptree.put(TAG::DST_NODE, m_data.dst_node);
    ptree.put(TAG::OLD_PKEY, ed25519_key2text(m_data.old_public_key, sizeof(m_data.old_public_key)));
    ptree.put(TAG::NEW_PKEY, ed25519_key2text(m_data.node_new_key, sizeof(m_data.node_new_key)));
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}
