#include "setnodestatus.h"
#include "helper/hash.h"
#include "helper/json.h"

SetNodeStatus::SetNodeStatus()
    : m_data{TXSTYPE_SBS} {
    m_responseError = ErrorCodes::Code::eNone;
}

SetNodeStatus::SetNodeStatus(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t ttime, uint16_t bbank, uint32_t status)
    : m_data{TXSTYPE_SBS, abank, auser, amsid, ttime, bbank, status} {
    m_responseError = ErrorCodes::Code::eNone;
}

int SetNodeStatus::getType() {
    return TXSTYPE_SBS;
}

unsigned char* SetNodeStatus::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* SetNodeStatus::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void SetNodeStatus::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void SetNodeStatus::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int SetNodeStatus::getDataSize() {
    return sizeof(m_data.info);
}

int SetNodeStatus::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* SetNodeStatus::getSignature() {
    return m_data.sign;
}

int SetNodeStatus::getSignatureSize() {
    return sizeof(m_data.sign);
}

void SetNodeStatus::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());
}

bool SetNodeStatus::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return (ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), pk, getSignature()) == 0);
}

user_t& SetNodeStatus::getUserInfo() {
    return m_response.usera;
}

uint32_t SetNodeStatus::getTime() {
    return m_data.info.ttime;
}

uint32_t SetNodeStatus::getUserId() {
    return m_data.info.auser;
}

uint32_t SetNodeStatus::getBankId() {
    return m_data.info.abank;
}

int64_t SetNodeStatus::getFee() {
    return TXS_SBS_FEE;
}

int64_t SetNodeStatus::getDeduct() {
    return 0;
}

bool SetNodeStatus::send(INetworkClient& netClient)
{
    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("SetNodeStatus sending error\n");
        return false;
    }

    if(!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("SetNodeStatus reading error\n");
        return false;
    }

    if(m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("SetNodeStatus ERROR reading global info\n");
        return false;
    }

    return true;
}

void SetNodeStatus::saveResponse(settings& sts) {
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

std::string SetNodeStatus::toString(bool /*pretty*/) {
    return "";
}

void SetNodeStatus::toJson(boost::property_tree::ptree &ptree) {
    if (!m_responseError) {
        Helper::print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        Helper::print_msgid_info(ptree, m_data.info.abank, m_response.msid, m_response.mpos);
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void SetNodeStatus::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.abank);
    ptree.put(TAG::SRC_USER, m_data.info.auser);
    ptree.put(TAG::MSGID, m_data.info.amsid);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::DST_NODE, m_data.info.bbank);
    ptree.put(TAG::STATUS, m_data.info.status);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

uint32_t SetNodeStatus::getUserMessageId() {
    return m_data.info.amsid;
}

uint32_t SetNodeStatus::getDestBankId() {
    return m_data.info.bbank;
}

uint32_t SetNodeStatus::getStatus() {
    return m_data.info.status;
}
