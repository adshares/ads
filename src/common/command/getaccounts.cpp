#include <fcntl.h>
#include <sys/stat.h>
#include "getaccounts.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/blocks.h"
#include "helper/blockfilereader.h"

GetAccounts::GetAccounts()
    : m_data{} {
}

GetAccounts::GetAccounts(uint16_t src_bank, uint32_t src_user, uint32_t block, uint16_t dst_bank, uint32_t time)
    : m_data(src_bank, src_user, time, block, dst_bank) {
    if (!dst_bank) {
        m_data.info.dst_node = src_bank;
    }
}

unsigned char* GetAccounts::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetAccounts::getResponse() {
    return m_responseBuffer.get();
}

void GetAccounts::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetAccounts::setResponse(char* /*response*/) {
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

CommandType GetAccounts::getCommandType() {
    return CommandType::eReadingOnly;
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

bool GetAccounts::send(INetworkClient& netClient) {
    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("GetAccounts sending error\n");
        return false;
    }

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("GetAccounts reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    if (!netClient.readData((int32_t*)&m_responseBufferLength, sizeof(uint32_t))) {
        ELOG("Get accounts reading error\n");
        return false;
    }

    // read rest of buffer, all accounts
    m_responseBuffer = std::make_unique<unsigned char[] >(m_responseBufferLength);
    if (!netClient.readData(m_responseBuffer.get(), m_responseBufferLength)) {
        ELOG("GetAccounts ERROR reading response\n");
        return false;
    }
    return true;
}

uint32_t GetAccounts::getDestBankId() {
    return m_data.info.dst_node;
}

uint32_t GetAccounts::getBlockId() {
    return m_data.info.block;
}

uint32_t GetAccounts::getUserMessageId() {
    return 0;
}

std::string GetAccounts::toString(bool /*pretty*/) {
    return "";
}

void GetAccounts::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        uint32_t no_of_users = this->getResponseSize() / sizeof(user_t);
        user_t* user_ptr=(user_t*)this->getResponse();
        uint32_t bankId = this->getDestBankId();
        boost::property_tree::ptree users;
        for(uint32_t i=0; i<no_of_users; i++, user_ptr++) {
            boost::property_tree::ptree user;
            print_user(*user_ptr, user, true, bankId, i);
            users.push_back(std::make_pair("", user.get_child("account")));
        }
        ptree.put_child("accounts", users);
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

ErrorCodes::Code GetAccounts::prepareResponse(uint32_t lastPath, uint32_t lastUsers) {
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    uint32_t destBank = this->getDestBankId();
    char filename[64];
    int fd;
    std::shared_ptr<Helper::BlockFileReader> undo;

    sprintf(filename,"usr/%04X.dat", destBank);
    fd = open(filename,O_RDONLY);
    if (fd < 0) {
        errorCode = ErrorCodes::Code::eBankNotFound;
    } else {
        Helper::FileName::getUndo(filename, lastPath, destBank);
        undo = std::make_shared<Helper::BlockFileReader>(filename);
        if(!undo->isOpen()) {
            errorCode = ErrorCodes::Code::eUndoNotFound;
        }
    }

    if (!errorCode) {
        m_responseBufferLength = lastUsers * sizeof(user_t);
        //m_responseBuffer = new unsigned char[m_responseBufferLength];
        m_responseBuffer = std::make_unique<unsigned char[] >(m_responseBufferLength);
        uint8_t *dp = m_responseBuffer.get();
        for(uint32_t user=0; user<lastUsers; user++,dp+=sizeof(user_t)) {
            user_t u;
            u.msid=0;
            read(fd,dp,sizeof(user_t));
            undo->read(&u,sizeof(user_t));
            if(u.msid!=0) {
                memcpy(dp,&u,sizeof(user_t));
            }
        }
    }
    close(fd);

    return errorCode;
}

void GetAccounts::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.src_node);
    ptree.put(TAG::SRC_USER, m_data.info.src_user);
    ptree.put(TAG::DST_NODE, m_data.info.dst_node);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::BLOCK, m_data.info.block);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

std::string GetAccounts::usageHelperToString() {
    std::stringstream ss{};
    ss << "Usage: " << "{\"run\":\"get_accounts\",[\"node\":<node id>],[\"block\":<block_time as hex>]}" << "\n";
    ss << "Example: " << "{\"run\":\"get_accounts\",\"node\":\"1\",\"block\":\"5B052CE0\"}" << "\n";
    return ss.str();
}
