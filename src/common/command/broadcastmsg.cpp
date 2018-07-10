#include "broadcastmsg.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/hash.h"

BroadcastMsg::BroadcastMsg()
    : m_data{}, m_message(nullptr) {
}

BroadcastMsg::BroadcastMsg(uint16_t src_bank, uint32_t src_user, uint32_t msg_id, uint16_t msg_length, const char* msg, uint32_t time)
    : m_data(src_bank, src_user, msg_id, time, msg_length) {
    if (msg_length > MAX_BROADCAST_LENGTH) {
        throw std::runtime_error(ErrorCodes().getErrorMsg(ErrorCodes::Code::eBroadcastMaxLength));
    }
    m_message = new unsigned char[msg_length];
    memcpy(m_message, msg, msg_length);
}


BroadcastMsg::~BroadcastMsg() {
    delete[] m_message;
}

unsigned char* BroadcastMsg::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* BroadcastMsg::getAdditionalData() {
    if (!m_message && this->getAdditionalDataSize() > 0) {
        m_message = new unsigned char[this->getAdditionalDataSize()];
    }
    return m_message;
}

int BroadcastMsg::getAdditionalDataSize() {
    return m_data.info.msg_length;
}

unsigned char* BroadcastMsg::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void BroadcastMsg::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
    char *data_ptr = data + getDataSize();
    std::copy(data_ptr, data_ptr + getAdditionalDataSize(), getAdditionalData());
    data_ptr += getAdditionalDataSize();
    std::copy(data_ptr, data_ptr + getSignatureSize(), getSignature());
}

void BroadcastMsg::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int BroadcastMsg::getDataSize() {
    return sizeof(m_data.info);
}

int BroadcastMsg::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* BroadcastMsg::getSignature() {
    return m_data.sign;
}

int BroadcastMsg::getSignatureSize() {
    return sizeof(m_data.sign);
}

int BroadcastMsg::getType() {
    return TXSTYPE_BRO;
}

CommandType BroadcastMsg::getCommandType() {
    return CommandType::eModifying;
}

void BroadcastMsg::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    int dataSize = this->getDataSize();
    int additionalSize = this->getAdditionalDataSize();
    int size = dataSize + additionalSize;
    unsigned char* fullDataBuffer = new unsigned char[size];

    memcpy(fullDataBuffer, this->getData(), dataSize);
    memcpy(fullDataBuffer + dataSize, this->getAdditionalData(), additionalSize);
    ed25519_sign2(hash, SHA256_DIGEST_LENGTH, fullDataBuffer, size, sk, pk, getSignature());
    delete[] fullDataBuffer;
}

bool BroadcastMsg::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    int dataSize = this->getDataSize();
    int additionalSize = this->getAdditionalDataSize();
    int size = dataSize + additionalSize;
    unsigned char* fullDataBuffer = new unsigned char[size];

    if (!m_message) {
        DLOG("Message field empty\n");
    }

    memcpy(fullDataBuffer, this->getData(), dataSize);
    memcpy(fullDataBuffer + dataSize, this->getAdditionalData(), additionalSize);

    int result = ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH, fullDataBuffer, size, pk, getSignature());
    delete[] fullDataBuffer;
    return (result == 0);
}

void BroadcastMsg::saveResponse(settings& sts) {
    if (!std::equal(sts.pk, sts.pk + SHA256_DIGEST_LENGTH, m_response.usera.pkey)) {
        m_responseError = ErrorCodes::Code::ePkeyDiffers;
    }

    std::array<uint8_t, SHA256_DIGEST_LENGTH> hashout;
    Helper::create256signhash(getSignature(), getSignatureSize(), sts.ha, hashout);
    if (!sts.send_again && !std::equal(hashout.begin(), hashout.end(), m_response.usera.hash)) {
        m_responseError = ErrorCodes::Code::eHashMismatch;
    }

    sts.msid = m_response.usera.msid;
    std::copy(m_response.usera.hash, m_response.usera.hash + SHA256_DIGEST_LENGTH, sts.ha.data());
}

uint32_t BroadcastMsg::getUserId() {
    return m_data.info.src_user;
}

uint32_t BroadcastMsg::getBankId() {
    return m_data.info.src_node;
}

uint32_t BroadcastMsg::getTime() {
    return m_data.info.ttime;
}

int64_t BroadcastMsg::getFee() {
    return TXS_BRO_FEE(m_data.info.msg_length);
}

int64_t BroadcastMsg::getDeduct() {
    return 0;
}

uint32_t BroadcastMsg::getUserMessageId() {
    return m_data.info.msg_id;
}

bool BroadcastMsg::send(INetworkClient& netClient) {
    if(!netClient.sendData(getData(), getDataSize())) {
        ELOG("BroadcastMsg ERROR sending data\n");
        return false;
    }

    if(!netClient.sendData(getAdditionalData(), this->getAdditionalDataSize())) {
        ELOG("BroadcastMsg ERROR sending additional data\n");
        return false;
    }

    if(!netClient.sendData(getSignature(), this->getSignatureSize())) {
        ELOG("BroadcastMsg ERROR sending signature\n");
        return false;
    }

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("BroadcastMsg reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("BroadcastMsg ERROR reading global info\n");
        return false;
    }

    return true;
}

std::string BroadcastMsg::toString(bool /*pretty*/) {
    return "";
}

void BroadcastMsg::toJson(boost::property_tree::ptree& ptree) {
    if (m_responseError) {
        if (m_responseError == ErrorCodes::Code::ePkeyDiffers) {
            std::stringstream tx_user_hashin{};
            Helper::ed25519_key2text(tx_user_hashin, m_response.usera.pkey, SHA256_DIGEST_LENGTH);
            ptree.put("tx.account_public_key_new", tx_user_hashin.str());
        }
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    } else {
        Helper::print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        Helper::print_msgid_info(ptree, m_data.info.src_node, m_response.msid, m_response.mpos);
    }
}

void BroadcastMsg::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.src_node);
    ptree.put(TAG::SRC_USER, m_data.info.src_user);
    ptree.put(TAG::MSGID, m_data.info.msg_id);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::MSG_LEN, m_data.info.msg_length);
    if (m_message) {
        ptree.put(TAG::MSG, Helper::ed25519_key2text(m_message, m_data.info.msg_length));
    }
    ptree.put(TAG::SIGN, Helper::ed25519_key2text(getSignature(), getSignatureSize()));
}
