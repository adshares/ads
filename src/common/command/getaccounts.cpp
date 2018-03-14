#include "getaccounts.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"

GetAccounts::GetAccounts()
    : m_data{}, m_responseBuffer(nullptr), m_settings(nullptr) {
}

GetAccounts::GetAccounts(uint16_t src_bank, uint32_t src_user, uint32_t block, uint16_t dst_bank, uint32_t time)
    : m_data(src_bank, src_user, time, block, dst_bank), m_responseBuffer(nullptr), m_settings(nullptr) {
}

GetAccounts::~GetAccounts() {
    delete[] m_responseBuffer;
}

unsigned char* GetAccounts::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetAccounts::getResponse() {
    return m_responseBuffer;
}

void GetAccounts::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetAccounts::setResponse(char* response) {
    m_responseBuffer = reinterpret_cast<unsigned char*>(response);
}

int GetAccounts::getDataSize() {
    return sizeof(m_data.info);
}

int GetAccounts::getResponseSize() {
    return m_responseBufferLength;
}

unsigned char* GetAccounts::getSignature() {
    return m_data.sign;
}

int GetAccounts::getSignatureSize() {
    return sizeof(m_data.sign);
}

int GetAccounts::getType() {
    return TXSTYPE_NOD;
}

void GetAccounts::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetAccounts::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return (ed25519_sign_open(getData(), getDataSize(), pk, getSignature()) == 0);
}

void GetAccounts::saveResponse(settings& sts) {
    m_settings = &sts;
}

uint32_t GetAccounts::getUserId() {
    return m_data.info.src_user;
}

uint32_t GetAccounts::getBankId() {
    return m_data.info.src_node;
}

uint32_t GetAccounts::getTime() {
    return m_data.info.ttime;
}

int64_t GetAccounts::getFee() {
    return 0;
}

int64_t GetAccounts::getDeduct() {
    return 0;
}

user_t& GetAccounts::getUserInfo() {
    // in this case there is a multiple user info fields
    return *(user_t*)nullptr;
}

bool GetAccounts::send(INetworkClient& netClient) {
    if(!netClient.sendData(getData(), sizeof(m_data))) {
        return false;
    }

    char respLen[4];
    if (!netClient.readData(respLen, 4)) {
        std::cerr<<"GetAccounts ERROR reading response length\n";
        return false;
    }
    memcpy(&m_responseBufferLength, respLen, 4);
    m_responseBuffer = new unsigned char[m_responseBufferLength];

    if (!netClient.readData(m_responseBuffer, m_responseBufferLength)) {
        std::cerr<<"GetAccounts ERROR reading response\n";
    }

    return true;
}

uint32_t GetAccounts::getDestBankId() {
    return m_data.info.dst_node;
}

uint32_t GetAccounts::getBlockId() {
    return m_data.info.block;
}

std::string GetAccounts::toString(bool /*pretty*/) {
    return "";
}

boost::property_tree::ptree GetAccounts::toJson() {
    if (!m_settings) {
        return boost::property_tree::ptree();
    }

    user_t* user_ptr=(user_t*)m_responseBuffer;
    uint32_t no_of_users=m_responseBufferLength/sizeof(user_t);
    boost::property_tree::ptree users;
    for(uint32_t i=0; i<no_of_users; i++, user_ptr++) {
        boost::property_tree::ptree user;
        print_user(*user_ptr, user, true, this->getBankId(), i, *m_settings);
        users.push_back(std::make_pair("",user.get_child("account")));
    }
    std::cerr<<"Size: "<<users.size() <<std::endl;
    return users;
}
