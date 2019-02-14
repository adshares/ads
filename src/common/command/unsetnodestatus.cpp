#include "unsetnodestatus.h"
#include "helper/hash.h"
#include "helper/json.h"

UnsetNodeStatus::UnsetNodeStatus()
    : m_data{TXSTYPE_UBS} {
    m_responseError = ErrorCodes::Code::eNone;
}

UnsetNodeStatus::UnsetNodeStatus(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t ttime, uint16_t bbank, uint32_t status)
    : m_data{TXSTYPE_UBS, abank, auser, amsid, ttime, bbank, status} {
    m_responseError = ErrorCodes::Code::eNone;
}

int UnsetNodeStatus::getType() {
    return TXSTYPE_UBS;
}

CommandType UnsetNodeStatus::getCommandType() {
    return CommandType::eModifying;
}

unsigned char* UnsetNodeStatus::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* UnsetNodeStatus::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void UnsetNodeStatus::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void UnsetNodeStatus::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int UnsetNodeStatus::getDataSize() {
    return sizeof(m_data.info);
}

int UnsetNodeStatus::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* UnsetNodeStatus::getSignature() {
    return m_data.sign;
}

int UnsetNodeStatus::getSignatureSize() {
    return sizeof(m_data.sign);
}

void UnsetNodeStatus::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), sk, pk, getSignature());
}

bool UnsetNodeStatus::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return (ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH, getData(), getDataSize(), pk, getSignature()) == 0);
}

uint32_t UnsetNodeStatus::getTime() {
    return m_data.info.ttime;
}

uint32_t UnsetNodeStatus::getUserId() {
    return m_data.info.auser;
}

uint32_t UnsetNodeStatus::getBankId() {
    return m_data.info.abank;
}

int64_t UnsetNodeStatus::getFee() {
    return TXS_UBS_FEE;
}

int64_t UnsetNodeStatus::getDeduct() {
    return 0;
}

bool UnsetNodeStatus::send(INetworkClient& netClient)
{
    if(!sendData(netClient)) {
        return false;
    }

    readDataSize(netClient);

    if(!readResponseError(netClient)) {
        ELOG("UnsetNodeStatus reading error\n");
        return false;
    }

    if(m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("UnsetNodeStatus ERROR reading global info\n");
        return false;
    }

    return true;
}

void UnsetNodeStatus::saveResponse(settings& sts) {
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

std::string UnsetNodeStatus::toString(bool /*pretty*/) {
    return "";
}

void UnsetNodeStatus::toJson(boost::property_tree::ptree &ptree) {
    if (!m_responseError) {
        print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        print_msgid_info(ptree, m_data.info.abank, m_response.msid, m_response.mpos);
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
        ptree.put(ERROR_CODE_TAG, m_responseError);
        ptree.put(ERROR_INFO_TAG, m_responseInfo);
    }
}

void UnsetNodeStatus::txnToJson(boost::property_tree::ptree& ptree) {
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

uint32_t UnsetNodeStatus::getUserMessageId() {
    return m_data.info.amsid;
}

uint32_t UnsetNodeStatus::getDestBankId() {
    return m_data.info.bbank;
}

uint32_t UnsetNodeStatus::getStatus() {
    return m_data.info.status;
}

std::string  UnsetNodeStatus::usageHelperToString() {
    std::stringstream ss{};
    ss << "Usage: " << "{\"run\":\"unset_node_status\",\"node\":<node id>,\"status\":<bits_to_unset>}" << "\n";
    ss << "Example: " << "{\"run\":\"unset_node_status\",\"node\":\"1\",\"status\":\"8\"}" << "\n";
    return ss.str();
}
