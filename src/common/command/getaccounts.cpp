#include "getaccounts.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"

GetAccounts::GetAccounts()
    : m_data{}, m_responseBuffer(nullptr) {
}

GetAccounts::GetAccounts(uint16_t src_bank, uint32_t src_user, uint32_t block, uint16_t dst_bank, uint32_t time)
    : m_data(src_bank, src_user, time, block, dst_bank), m_responseBuffer(nullptr) {
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

void GetAccounts::saveResponse(settings& /*sts*/) {
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
    if (m_responseBufferLength < sizeof(user_t)) {
        // response length is error code
        m_responseBuffer = new unsigned char[4];
        memcpy(m_responseBuffer, respLen, 4);
        m_responseBufferLength = 4;
    } else {
        // read rest of buffer, all accounts
        m_responseBuffer = new unsigned char[m_responseBufferLength];

        if (!netClient.readData(m_responseBuffer, m_responseBufferLength)) {
            std::cerr<<"GetAccounts ERROR reading response\n";
            return false;
        }
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
    return boost::property_tree::ptree();
}

ErrorCodes::Code GetAccounts::prepareResponse(uint32_t lastPath, uint32_t lastUsers) {
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    uint32_t destBank = this->getDestBankId();
    char filename[64];
    int fd, ud;

    sprintf(filename,"usr/%04X.dat", destBank);
    fd = open(filename,O_RDONLY);
    if (fd < 0) {
        errorCode = ErrorCodes::Code::eBankNotFound;
    } else {
        sprintf(filename,"blk/%03X/%05X/und/%04X.dat",lastPath>>20,lastPath&0xFFFFF, destBank);
        ud = open(filename,O_RDONLY);
        if(ud < 0) {
            errorCode = ErrorCodes::Code::eUndoNotFound;
            close(fd);
        }
    }

    if (errorCode) {
        memcpy(m_responseBuffer, (uint8_t*)&errorCode, 4);
        m_responseBufferLength = 4;
        return errorCode;
    }

    m_responseBufferLength = lastUsers * sizeof(user_t);
    m_responseBuffer = new unsigned char[4+m_responseBufferLength];
    uint8_t *dp = m_responseBuffer+4;
    for(uint32_t user=0; user<lastUsers; user++,dp+=sizeof(user_t)) {
        user_t u;
        u.msid=0;
        read(fd,dp,sizeof(user_t));
        read(ud,&u,sizeof(user_t));
        if(u.msid!=0) {
            memcpy(dp,&u,sizeof(user_t));
        }
    }
    close(ud);
    close(fd);
    memcpy(m_responseBuffer, &m_responseBufferLength, 4);
    return errorCode;
}
