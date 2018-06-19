#include "setaccountstatus.h"
#include "helper/hash.h"
#include "helper/json.h"

SetAccountStatus::SetAccountStatus()
    : m_data{TXSTYPE_SUS} {
    m_responseError = ErrorCodes::Code::eNone;
}

SetAccountStatus::SetAccountStatus(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t ttime, uint16_t bbank, uint32_t buser, uint16_t status)
    : m_data{TXSTYPE_SUS, abank, auser, amsid, ttime, bbank, buser, status} {
    m_responseError = ErrorCodes::Code::eNone;
}

int SetAccountStatus::getType() {
    return TXSTYPE_SUS;
}

CommandType SetAccountStatus::getCommandType() {
    return CommandType::eModifying;
}

unsigned char* SetAccountStatus::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* SetAccountStatus::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void SetAccountStatus::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void SetAccountStatus::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int SetAccountStatus::getDataSize() {
    return sizeof(m_data.info);
}

int SetAccountStatus::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* SetAccountStatus::getSignature() {
    return m_data.sign;
}

int SetAccountStatus::getSignatureSize() {
    return sizeof(m_data.sign);
}

void SetAccountStatus::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());
}

bool SetAccountStatus::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return (ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), pk, getSignature()) == 0);
}

user_t& SetAccountStatus::getUserInfo() {
    return m_response.usera;
}

uint32_t SetAccountStatus::getTime() {
    return m_data.info.ttime;
}

uint32_t SetAccountStatus::getUserId() {
    return m_data.info.auser;
}

uint32_t SetAccountStatus::getBankId() {
    return m_data.info.abank;
}

int64_t SetAccountStatus::getFee() {
    return TXS_SUS_FEE;
}

int64_t SetAccountStatus::getDeduct() {
    return 0;
}

bool SetAccountStatus::send(INetworkClient& netClient)
{
    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("SetAccountStatus sending error\n");
        return false;
    }

    if(!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("SetAccountStatus reading error\n");
        return false;
    }

    if(m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("SetAccountStatus ERROR reading global info\n");
        return false;
    }

    return true;
}

void SetAccountStatus::saveResponse(settings& sts) {
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

std::string SetAccountStatus::toString(bool /*pretty*/) {
    return "";
}

void SetAccountStatus::toJson(boost::property_tree::ptree &ptree) {
    if (!m_responseError) {
        Helper::print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        Helper::print_msgid_info(ptree, m_data.info.abank, m_response.msid, m_response.mpos);
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void SetAccountStatus::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.abank);
    ptree.put(TAG::SRC_USER, m_data.info.auser);
    ptree.put(TAG::MSGID, m_data.info.amsid);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::DST_NODE, m_data.info.bbank);
    ptree.put(TAG::DST_USER, m_data.info.buser);
    ptree.put(TAG::STATUS, m_data.info.status);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

uint32_t SetAccountStatus::getUserMessageId() {
    return m_data.info.amsid;
}

uint32_t SetAccountStatus::getDestBankId() {
    return m_data.info.bbank;
}

uint32_t SetAccountStatus::getDestUserId() {
    return m_data.info.buser;
}

uint16_t SetAccountStatus::getStatus() {
    return m_data.info.status;
}
