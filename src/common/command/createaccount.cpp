#include "createaccount.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/hash.h"
#include "helper/json.h"

CreateAccount::CreateAccount()
    : m_data{} {
    m_newAccount.user_id = 0;
    m_responseError = ErrorCodes::Code::eNone;
}

CreateAccount::CreateAccount(uint16_t src_bank, uint32_t src_user, uint32_t msg_id, uint16_t dst_bank, uint32_t time)
    : m_data(src_bank, src_user, msg_id, time, dst_bank) {
    m_newAccount.user_id = 0;
    m_responseError = ErrorCodes::Code::eNone;
}

unsigned char* CreateAccount::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* CreateAccount::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void CreateAccount::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void CreateAccount::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int CreateAccount::getDataSize() {
    return sizeof(m_data.info);
}

int CreateAccount::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* CreateAccount::getSignature() {
    return m_data.sign;
}

int CreateAccount::getSignatureSize() {
    return sizeof(m_data.sign);
}

int CreateAccount::getType() {
    return TXSTYPE_USR;
}

void CreateAccount::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash,SHA256_DIGEST_LENGTH , getData(), getDataSize(), sk, pk, getSignature());
}

bool CreateAccount::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return (ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH,getData(),getDataSize(),pk,getSignature()) == 0);

}

void CreateAccount::saveResponse(settings& sts) {
    sts.msid = m_response.usera.msid;
    std::copy(m_response.usera.hash, m_response.usera.hash + SHA256_DIGEST_LENGTH, sts.ha.data());
}

uint32_t CreateAccount::getUserId() {
    return m_data.info.src_user;
}

uint32_t CreateAccount::getBankId() {
    return m_data.info.src_node;
}

uint32_t CreateAccount::getTime() {
    return m_data.info.ttime;
}

int64_t CreateAccount::getFee() {
    int64_t fee;
    if (this->getBankId() == this->getDestBankId()) {
        fee = TXS_MIN_FEE;
    } else {
        fee = TXS_USR_FEE;
    }
    return fee;
}

int64_t CreateAccount::getDeduct() {
    return USER_MIN_MASS;
}

user_t& CreateAccount::getUserInfo() {
    return m_response.usera;
}

bool CreateAccount::send(INetworkClient& netClient) {
    if(!netClient.sendData(getData(), sizeof(m_data))) {
        std::cerr<<"CreateAccount sending error\n";
        return false;
    }

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
       std::cerr<<"CreateAccount reading error\n";
       return false;
    }

    if (m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        std::cerr<<"CreateAccount ERROR reading global info\n";
        return false;
    }

    return true;
}

unsigned char* CreateAccount::getAdditionalData() {
    return reinterpret_cast<unsigned char*>(&m_newAccount);
}

void CreateAccount::setAdditionalData(char* data) {
    m_newAccount = *reinterpret_cast<decltype(m_newAccount)*>(data);
}

int CreateAccount::getAdditionalDataSize() {
    // here return size of struct only when it was set on server side
    int size = 0;
    if (m_newAccount.user_id) {
        size = sizeof(m_newAccount);
    }
    return size;
}

uint32_t CreateAccount::getDestBankId() {
    return m_data.info.dst_node;
}

uint32_t CreateAccount::getUserMessageId() {
    return m_data.info.msg_id;
}

void CreateAccount::setNewUser(uint32_t userId, uint8_t userPkey[SHA256_DIGEST_LENGTH]) {
    m_newAccount = NewAccountData(userId, userPkey);
}

std::string CreateAccount::toString(bool /*pretty*/) {
    return "";
}

void CreateAccount::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        // only for account created in the same node
        if (m_response.usera.user) {
            char nAccountAddress[19]="";
            uint16_t suffix=Helper::crc_acnt(getBankId(), m_response.usera.user);
            suffix=crc_acnt(getBankId(), m_response.usera.user);
            sprintf(nAccountAddress, "%04X-%08X-%04X", getBankId(), m_response.usera.user, suffix);
            ptree.put("new_account.address", nAccountAddress);
            ptree.put("new_account.node", this->getBankId());
            ptree.put("new_account.id", m_response.usera.user);
        }
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}
