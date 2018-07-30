#include "sendmany.h"

#include <set>

#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/hash.h"

SendMany::SendMany()
    : m_data{}, m_additionalData(nullptr) {
    m_responseError = ErrorCodes::eNone;
}

SendMany::SendMany(uint16_t bank, uint32_t user, uint32_t msid, std::vector<SendAmountTxnRecord> &txns_data, uint32_t time)
    : m_data(bank, user, msid, time, txns_data.size()), m_transactions(txns_data), m_additionalData(nullptr) {
    m_responseError = ErrorCodes::eNone;
    fillAdditionalData();
}

SendMany::~SendMany() {
    delete[] m_additionalData;
}

unsigned char* SendMany::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* SendMany::getAdditionalData() {
    // workaround: from server side buffer needs to be alocated before read additional data
    if (!m_additionalData && this->getAdditionalDataSize() > 0) {
        m_additionalData = new unsigned char[this->getAdditionalDataSize()];
    }
    return m_additionalData;
}

int SendMany::getAdditionalDataSize() {
    return (m_data.info.txn_counter * sizeof(SendAmountTxnRecord));
}

unsigned char* SendMany::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void SendMany::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
    char *data_ptr = data + getDataSize();
    std::copy(data_ptr, data_ptr + getAdditionalDataSize(), getAdditionalData());
    data_ptr += getAdditionalDataSize();
    std::copy(data_ptr, data_ptr + getSignatureSize(), getSignature());
    initTransactionVector();
}

void SendMany::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int SendMany::getDataSize() {
    return sizeof(m_data.info);
}

int SendMany::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* SendMany::getSignature() {
    return m_data.sign;
}

int SendMany::getSignatureSize() {
    return sizeof(m_data.sign);
}

int SendMany::getType() {
    return TXSTYPE_MPT;
}

CommandType SendMany::getCommandType() {
    return CommandType::eModifying;
}

void SendMany::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    int dataSize = this->getDataSize();
    int additionalSize = this->getAdditionalDataSize();
    int size = dataSize + additionalSize;
    unsigned char *fullDataBuffer = new unsigned char[size];

    if (!m_additionalData) {
        fillAdditionalData();
    }

    memcpy(fullDataBuffer, this->getData(), dataSize);
    memcpy(fullDataBuffer + dataSize, this->getAdditionalData(), additionalSize);

    ed25519_sign2(hash,SHA256_DIGEST_LENGTH , fullDataBuffer, size, sk, pk, getSignature());
    delete[] fullDataBuffer;
}

bool SendMany::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    int dataSize = this->getDataSize();
    int additionalSize = this->getAdditionalDataSize();
    int size = dataSize + additionalSize;
    unsigned char *fullDataBuffer = new unsigned char[size];

    if (!m_additionalData) {
        fillAdditionalData();
    }

    memcpy(fullDataBuffer, this->getData(), dataSize);
    memcpy(fullDataBuffer + dataSize, this->getAdditionalData(), additionalSize);

    int result = ed25519_sign_open2(hash,SHA256_DIGEST_LENGTH, fullDataBuffer, size, pk, getSignature());
    delete[] fullDataBuffer;
    return (result == 0);

}

void SendMany::saveResponse(settings& sts) {
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

uint32_t SendMany::getUserId() {
    return m_data.info.src_user;
}

uint32_t SendMany::getBankId() {
    return m_data.info.src_node;
}

uint32_t SendMany::getTime() {
    return m_data.info.txn_time;
}

int64_t SendMany::getFee() {
    // TODO: CommandHandler::validateModifyingCommand is called before onInit so m_transactions could be not initialized
    initTransactionVector();

    uint64_t fee = 0;
    for (auto &it : m_transactions) {
        fee+=TXS_MPT_FEE(it.amount);
        if(m_data.info.src_node!=it.dest_node) {
            fee+=TXS_LNG_FEE(it.amount);
        }
    }
    if (fee < TXS_MINMPT_FEE(m_transactions.size())) {
        fee = TXS_MINMPT_FEE(m_transactions.size());
    }
    return fee;
}

int64_t SendMany::getDeduct() {
    // TODO: CommandHandler::validateModifyingCommand is called before onInit so m_transactions could be not initialized
    initTransactionVector();

    int64_t deduct = 0;
    for (auto &it : m_transactions) {
        deduct += it.amount;
    }
    return deduct;
}

bool SendMany::send(INetworkClient& netClient) {
    if(!netClient.sendData(getData(), this->getDataSize())) {
        ELOG("SendMany ERROR sending data\n");
        return false;
    }

    if(!netClient.sendData(getAdditionalData(), this->getAdditionalDataSize())) {
        ELOG("SendMany ERROR sending additional data\n");
        return false;
    }

    if(!netClient.sendData(getSignature(), this->getSignatureSize())) {
        ELOG("SendMany ERROR sending signature\n");
        return false;
    }

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("SendMany reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("SendMany ERROR reading global info\n");
        return false;
    }

    return true;
}

uint32_t SendMany::getUserMessageId() {
    return m_data.info.msg_id;
}

void SendMany::fillAdditionalData() {
    if (!m_additionalData) {
        int size = this->getAdditionalDataSize();
        if (size > 0) {
            m_additionalData = new unsigned char[size];
            unsigned char* dataIt = m_additionalData;
            int itemSize = sizeof(SendAmountTxnRecord);
            for (auto it : m_transactions) {
                memcpy(dataIt, &it, itemSize);
                dataIt += itemSize;
            }
        }
    }
}

void SendMany::initTransactionVector() {
    if(m_transactions.size() > 0) {
        return;
    }
    int size = this->getAdditionalDataSize();
    if (m_additionalData && size > 0) {
        SendAmountTxnRecord txn_record;
        int shift = 0;
        while (shift < size) {
            memcpy(&txn_record, m_additionalData + shift, sizeof(SendAmountTxnRecord));
            m_transactions.push_back(txn_record);
            shift += sizeof(SendAmountTxnRecord);
        }
    }
}

void SendMany::checkForDuplicates() {
    initTransactionVector();
    std::set<std::pair<uint16_t, uint32_t>> checkForDuplicate;
    for (auto &it : m_transactions) {
        uint16_t node = it.dest_node;
        uint32_t user = it.dest_user;
        if (!checkForDuplicate.insert(std::make_pair(node, user)).second) {
            ELOG("ERROR: duplicate target: %04X:%08X\n", node, user);
            throw ErrorCodes::Code::eDuplicatedTarget;
        }
        if (it.amount <= 0) {
            ELOG("ERROR: only positive transactions allowed in MPT\n");
            throw ErrorCodes::Code::eAmountNotPositive;
        }
    }
}

std::vector<SendAmountTxnRecord> SendMany::getTransactionsVector() {
    initTransactionVector();
    return m_transactions;
}

std::string SendMany::toString(bool /*pretty*/) {
    return "";
}

void SendMany::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        Helper::print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        Helper::print_msgid_info(ptree, m_data.info.src_node, m_response.msid, m_response.mpos);
    } else {
        if (m_responseError == ErrorCodes::Code::ePkeyDiffers) {
            std::stringstream tx_user_hashin{};
            Helper::ed25519_key2text(tx_user_hashin, m_response.usera.pkey, SHA256_DIGEST_LENGTH);
            ptree.put("tx.account_public_key_new", tx_user_hashin.str());
        }
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void SendMany::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.txn_type));
    ptree.put(TAG::SRC_NODE, m_data.info.src_node);
    ptree.put(TAG::SRC_USER, m_data.info.src_user);
    ptree.put(TAG::SRC_ADDRESS, Helper::print_address(m_data.info.src_node, m_data.info.src_user));
    ptree.put(TAG::MSGID, m_data.info.msg_id);
    ptree.put(TAG::TIME, m_data.info.txn_time);
    ptree.put(TAG::TXN_COUNTER, m_data.info.txn_counter);
    ptree.put(TAG::SENDER_FEE, print_amount(this->getFee()));
    if (m_data.info.txn_counter > 0) {
        boost::property_tree::ptree txns;
        for (auto &it : m_transactions) {
            boost::property_tree::ptree txn;
            txn.put(TAG::DST_NODE, it.dest_node);
            txn.put(TAG::DST_USER, it.dest_user);
            txn.put(TAG::DST_ADDRESS, Helper::print_address(it.dest_node, it.dest_user));
            txn.put(TAG::AMOUNT, print_amount(it.amount));
            txns.push_back(std::make_pair("", txn));
        }
        ptree.add_child("wires", txns);
    }

    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}
