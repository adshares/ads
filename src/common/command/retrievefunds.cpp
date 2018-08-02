#include "retrievefunds.h"
#include "helper/hash.h"
#include "helper/json.h"

RetrieveFunds::RetrieveFunds()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

RetrieveFunds::RetrieveFunds(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t ttime, uint16_t bbank, uint32_t buser)
    : m_data{abank, auser, amsid, ttime, bbank, buser} {
    m_responseError = ErrorCodes::Code::eNone;
}

int RetrieveFunds::getType() {
    return TXSTYPE_GET;
}

CommandType RetrieveFunds::getCommandType() {
    return CommandType::eModifying;
}

unsigned char* RetrieveFunds::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* RetrieveFunds::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void RetrieveFunds::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void RetrieveFunds::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int RetrieveFunds::getDataSize() {
    return sizeof(m_data.info);
}

int RetrieveFunds::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* RetrieveFunds::getSignature() {
    return m_data.sign;
}

int RetrieveFunds::getSignatureSize() {
    return sizeof(m_data.sign);
}

void RetrieveFunds::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());
}

bool RetrieveFunds::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return (ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), pk, getSignature()) == 0);
}

uint32_t RetrieveFunds::getTime() {
    return m_data.info.ttime;
}

uint32_t RetrieveFunds::getUserId() {
    return m_data.info.auser;
}

uint32_t RetrieveFunds::getBankId() {
    return m_data.info.abank;
}

int64_t RetrieveFunds::getFee() {
    return TXS_GET_FEE;
}

int64_t RetrieveFunds::getDeduct() {
    return 0;
}

bool RetrieveFunds::send(INetworkClient& netClient)
{
    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("RetrieveFunds sending error\n");
        return false;
    }

    if(!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("RetrieveFunds reading error\n");
        return false;
    }

    if(m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("RetrieveFunds ERROR reading global info\n");
        return false;
    }

    return true;
}

void RetrieveFunds::saveResponse(settings& sts) {
    if (!sts.without_secret && !sts.signature_provided && !std::equal(sts.pk, sts.pk + SHA256_DIGEST_LENGTH, m_response.usera.pkey)) {
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

std::string RetrieveFunds::toString(bool /*pretty*/) {
    return "";
}

void RetrieveFunds::toJson(boost::property_tree::ptree &ptree) {
    if (!m_responseError) {
        print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        Helper::print_msgid_info(ptree, m_data.info.abank, m_response.msid, m_response.mpos);
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void RetrieveFunds::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.abank);
    ptree.put(TAG::SRC_USER, m_data.info.auser);
    ptree.put(TAG::MSGID, m_data.info.amsid);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::DST_NODE, m_data.info.bbank);
    ptree.put(TAG::DST_USER, m_data.info.buser);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

uint32_t RetrieveFunds::getUserMessageId() {
    return m_data.info.amsid;
}

uint32_t RetrieveFunds::getDestBankId() {
    return m_data.info.bbank;
}

uint32_t RetrieveFunds::getDestUserId() {
    return m_data.info.buser;
}
