#include "unsetaccountstatus.h"
#include "helper/hash.h"
#include "helper/json.h"

UnsetAccountStatus::UnsetAccountStatus()
    : m_data{TXSTYPE_UUS} {
    m_responseError = ErrorCodes::Code::eNone;
}

UnsetAccountStatus::UnsetAccountStatus(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t ttime, uint16_t bbank, uint32_t buser, uint16_t status)
    : m_data{TXSTYPE_UUS, abank, auser, amsid, ttime, bbank, buser, status} {
    m_responseError = ErrorCodes::Code::eNone;
}

int UnsetAccountStatus::getType() {
    return TXSTYPE_UUS;
}

CommandType UnsetAccountStatus::getCommandType() {
    return CommandType::eModifying;
}

unsigned char* UnsetAccountStatus::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* UnsetAccountStatus::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void UnsetAccountStatus::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void UnsetAccountStatus::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int UnsetAccountStatus::getDataSize() {
    return sizeof(m_data.info);
}

int UnsetAccountStatus::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* UnsetAccountStatus::getSignature() {
    return m_data.sign;
}

int UnsetAccountStatus::getSignatureSize() {
    return sizeof(m_data.sign);
}

void UnsetAccountStatus::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());
}

bool UnsetAccountStatus::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return (ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), pk, getSignature()) == 0);
}

uint32_t UnsetAccountStatus::getTime() {
    return m_data.info.ttime;
}

uint32_t UnsetAccountStatus::getUserId() {
    return m_data.info.auser;
}

uint32_t UnsetAccountStatus::getBankId() {
    return m_data.info.abank;
}

int64_t UnsetAccountStatus::getFee() {
    return TXS_UUS_FEE;
}

int64_t UnsetAccountStatus::getDeduct() {
    return 0;
}

bool UnsetAccountStatus::send(INetworkClient& netClient)
{
    if(!sendData(netClient)) {
        return false;
    }

    readDataSize(netClient);

    if(!readResponseError(netClient)) {
        ELOG("UnsetAccountStatus reading error\n");
        return false;
    }

    if(m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("UnsetAccountStatus ERROR reading global info\n");
        return false;
    }

    return true;
}

void UnsetAccountStatus::saveResponse(settings& sts) {
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

std::string UnsetAccountStatus::toString(bool /*pretty*/) {
    return "";
}

void UnsetAccountStatus::toJson(boost::property_tree::ptree &ptree) {
    if (!m_responseError) {
        Helper::print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        Helper::print_msgid_info(ptree, m_data.info.abank, m_response.msid, m_response.mpos);
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
        ptree.put(ERROR_CODE_TAG, m_responseError);
        ptree.put(ERROR_INFO_TAG, m_responseInfo);
    }
}

void UnsetAccountStatus::txnToJson(boost::property_tree::ptree& ptree) {
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

uint32_t UnsetAccountStatus::getUserMessageId() {
    return m_data.info.amsid;
}

uint32_t UnsetAccountStatus::getDestBankId() {
    return m_data.info.bbank;
}

uint32_t UnsetAccountStatus::getDestUserId() {
    return m_data.info.buser;
}

uint16_t UnsetAccountStatus::getStatus() {
    return m_data.info.status;
}

std::string  UnsetAccountStatus::usageHelperToString() {
    std::stringstream ss{};
    ss << "Usage: " << "{\"run\":\"unset_account_status\",\"address\":<destination_account_id>,\"status\":<bits_to_unset>}" << "\n";
    ss << "Example: " << "{\"run\":\"unset_account_status\",\"address\":\"0001-00000000-XXXX\",\"status\":\"10\"}" << "\n";
    return ss.str();
}
