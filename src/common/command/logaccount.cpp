#include "logaccount.h"
#include "helper/hash.h"
#include "helper/json.h"

LogAccount::LogAccount()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

LogAccount::LogAccount(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t ttime)
    : m_data{abank, auser, amsid, ttime} {
    m_responseError = ErrorCodes::Code::eNone;
}

int LogAccount::getType() {
    return TXSTYPE_SAV;
}

CommandType LogAccount::getCommandType() {
    return CommandType::eModifying;
}

unsigned char* LogAccount::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* LogAccount::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void LogAccount::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
    char *data_ptr = data + getDataSize();
    std::copy(data_ptr, data_ptr + getSignatureSize(), getSignature());

    data_ptr += getSignatureSize();
    setAdditionalData(data_ptr);
}

void LogAccount::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int LogAccount::getDataSize() {
    return sizeof(m_data.info);
}

int LogAccount::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* LogAccount::getSignature() {
    return m_data.sign;
}

int LogAccount::getSignatureSize() {
    return sizeof(m_data.sign);
}

void LogAccount::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());
}

bool LogAccount::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return (ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), pk, getSignature()) == 0);
}

uint32_t LogAccount::getTime() {
    return m_data.info.ttime;
}

uint32_t LogAccount::getUserId() {
    return m_data.info.auser;
}

uint32_t LogAccount::getBankId() {
    return m_data.info.abank;
}

int64_t LogAccount::getFee() {
    return TXS_SAV_FEE;
}

int64_t LogAccount::getDeduct() {
    return 0;
}

bool LogAccount::send(INetworkClient& netClient)
{
    if(!netClient.sendData(getData(), getDataSize())) {
        ELOG("LogAccount ERROR sending data\n");
        return false;
    }

    if(!netClient.sendData(getAdditionalData(), getAdditionalDataSize())) {
        ELOG("LogAccount ERROR sending additional data\n");
        return false;
    }

    if(!netClient.sendData(getSignature(), getSignatureSize())) {
        ELOG("LogAccount ERROR sending signature\n");
        return false;
    }

    if(!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("LogAccount ERROR reading response error\n");
        return false;
    }

    if(m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("LogAccount ERROR reading global info\n");
        return false;
    }

    return true;
}

void LogAccount::saveResponse(settings& sts) {
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

std::string LogAccount::toString(bool /*pretty*/) {
    return "";
}

void LogAccount::toJson(boost::property_tree::ptree &ptree) {
    if (!m_responseError) {
        Helper::print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        Helper::print_msgid_info(ptree, m_data.info.abank, m_response.msid, m_response.mpos);
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void LogAccount::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.abank);
    ptree.put(TAG::SRC_USER, m_data.info.auser);
    ptree.put(TAG::MSGID, m_data.info.amsid);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
    print_user(m_userData, ptree, false, this->getBankId(), this->getUserId());
}

uint32_t LogAccount::getUserMessageId() {
    return m_data.info.amsid;
}

unsigned char* LogAccount::getAdditionalData() {
    return reinterpret_cast<unsigned char*>(&m_userData);
}

void LogAccount::setAdditionalData(char* data) {
    m_userData = *reinterpret_cast<decltype(m_userData)*>(data);
}

int LogAccount::getAdditionalDataSize() {
    return sizeof(m_userData);
}
