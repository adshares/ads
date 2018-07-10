#include "createnode.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/hash.h"

CreateNode::CreateNode()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

CreateNode::CreateNode(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t ttime) 
	: m_data(abank, auser, amsid, ttime) {
    m_responseError = ErrorCodes::Code::eNone;
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

CommandType CreateNode::getCommandType() {
    return CommandType::eModifying;
}

void CreateNode::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());
}

bool CreateNode::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return( ed25519_sign_open2( hash , SHA256_DIGEST_LENGTH , getData(), getDataSize(), pk, getSignature() ) == 0);
}

void CreateNode::saveResponse(settings& sts) {
    if (!sts.without_secret && !std::equal(sts.pk, sts.pk + SHA256_DIGEST_LENGTH, m_response.usera.pkey)) {
        m_responseError = ErrorCodes::Code::ePkeyDiffers;
    }

    std::array<uint8_t, SHA256_DIGEST_LENGTH> hashout;
    Helper::create256signhash(getSignature(), getSignatureSize(), sts.ha, hashout);
    if (!sts.signature_provided && !std::equal(hashout.begin(), hashout.end(), m_response.usera.hash)) {
        m_responseError = ErrorCodes::Code::eHashMismatch;
    }

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
    return BANK_MIN_UMASS + BANK_MIN_TMASS;
}

bool CreateNode::send(INetworkClient& netClient) {
    if(! netClient.sendData(getData(), getDataSize() + getSignatureSize() )) {
        ELOG("CreateNode sending error\n");
        return false;
    }

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("CreateNode reading error\n");
    }

    if (m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("CreateNode ERROR reading global info\n");
        return false;
    }

    return true;
}

uint32_t CreateNode::getUserMessageId() {
    return m_data.data.amsid;
}

std::string CreateNode::toString(bool /*pretty*/) {
    return "";
}

void CreateNode::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        Helper::print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        Helper::print_msgid_info(ptree, m_data.data.abank, m_response.msid, m_response.mpos);
    } else {
        if (m_responseError == ErrorCodes::Code::ePkeyDiffers) {
            std::stringstream tx_user_hashin{};
            Helper::ed25519_key2text(tx_user_hashin, m_response.usera.pkey, SHA256_DIGEST_LENGTH);
            ptree.put("tx.account_public_key_new", tx_user_hashin.str());
        }
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void CreateNode::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.data.ttype));
    ptree.put(TAG::SRC_NODE, m_data.data.abank);
    ptree.put(TAG::SRC_USER, m_data.data.auser);
    ptree.put(TAG::MSGID, m_data.data.amsid);
    ptree.put(TAG::TIME, m_data.data.ttime);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}
