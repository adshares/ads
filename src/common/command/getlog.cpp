#include "getlog.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/txsname.h"
#include <fcntl.h>
#include <sys/stat.h>

GetLog::GetLog()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

GetLog::GetLog(uint16_t bank, uint32_t user, uint32_t from, uint16_t dst_node, uint32_t dst_user, bool full, const char *txnTypeFilter)
        : m_data(bank, user, from, dst_node, dst_user, full), m_txnTypeFilter(-1) {
    m_responseError = ErrorCodes::Code::eNone;
    if (strlen(txnTypeFilter) > 0) {
        int id = Helper::getTxnLogTypeId(txnTypeFilter);
        if (id >= 0) {
            m_txnTypeFilter = id;
        } else {
            // txn type doesnt exists
            m_responseError = ErrorCodes::Code::eIncorrectTransaction;
        }
    }
    getLastLogsUpdate();
}

void GetLog::getLastLogsUpdate() {
    m_lastlog=m_data.info.from; //save requested log period
    fprintf(stderr,"LOG: print from %d (%08X)\n",m_data.info.from,m_data.info.from);
    m_data.info.from=0;
    char filename[64];
    mkdir("log",0755);

    uint16_t node_id = this->getDestBankId();
    uint32_t user_id = this->getDestUserId();
    if(!node_id && !user_id) {
        node_id = this->getBankId();
        user_id = this->getUserId();
    }

    snprintf(filename, sizeof(filename),"log/%04X_%08X.bin", node_id, user_id);
    int fd=open(filename,O_RDONLY);
    if(fd>=0 && lseek(fd,-(off_t)sizeof(log_t),SEEK_END)>=0) {
        read(fd,&m_data.info.from,sizeof(uint32_t));
        if(m_data.info.from>0) {
            m_data.info.from--;
        } // accept 1s overlap
        fprintf(stderr,"LOG: setting last time to %d (%08X)\n",m_data.info.from,m_data.info.from);
    }
    close(fd);
}

unsigned char* GetLog::getData() {
    return reinterpret_cast<unsigned char*>(&m_data);
}

unsigned char* GetLog::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void GetLog::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetLog::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int GetLog::getDataSize() {
    return sizeof(m_data.info) - (this->getClientVersion() == 1 ?  (sizeof(uint16_t)+sizeof(uint32_t)+sizeof(uint8_t)) : 0);
}

int GetLog::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* GetLog::getSignature() {
    return m_data.sign;
}

int GetLog::getSignatureSize() {
    return sizeof(m_data.sign);
}

int GetLog::getType() {
    return TXSTYPE_LOG;
}

CommandType GetLog::getCommandType() {
    return CommandType::eReadingOnly;
}

void GetLog::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetLog::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return( ed25519_sign_open(getData(), getDataSize(), pk, getSignature() ) == 0);
}

void GetLog::saveResponse(settings& /*sts*/) {
}

uint16_t GetLog::getDestBankId() {
    return m_data.info.dst_node;
}

uint32_t GetLog::getDestUserId() {
    return m_data.info.dst_user;
}

uint32_t GetLog::getUserId() {
    return m_data.info.user;
}

uint32_t GetLog::getBankId() {
    return m_data.info.node;
}

uint8_t GetLog::getFull() {
    return m_data.info.full;
}

uint32_t GetLog::getTime() {
    return m_data.info.from;
}

int64_t GetLog::getFee() {
    return 0;
}

int64_t GetLog::getDeduct() {
    return 0;
}

uint32_t GetLog::getUserMessageId() {
    return 0;
}

bool GetLog::send(INetworkClient& netClient) {
    if (m_responseError) {
        // case when filter transaction type doesn't exists
        return true;
    }
    
    if(!sendData(netClient)) {
        return false;
    }

    readDataSize(netClient);

    if(!readResponseError(netClient)) {
        ELOG("GetLog reading error\n");
    }

    if (m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("GetLog reading response error\n");
        return false;
    }

    int length;
    if (!netClient.readData((int32_t*)&length, sizeof(int))) {
        ELOG("GetLog reading logs length error\n");
        return false;
    }

    if (length == 0) {
        DLOG("No new log entries\n");
        return true;
    }

    log_t* logs = new log_t[length];
    if (!netClient.readData((char*)logs, length)) {
        ELOG("GetLog reading logs error\n");
        delete[] logs;
        return false;
    }

    uint16_t node_id = this->getDestBankId();
    uint32_t user_id = this->getDestUserId();
    if(!node_id && !user_id) {
        node_id = this->getBankId();
        user_id = this->getUserId();
    }

    Helper::save_log(logs, length, m_data.info.from, node_id, user_id);

    delete[] logs;
    return true;
}

std::string GetLog::toString(bool /*pretty*/) {
    return "";
}

void GetLog::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        uint16_t node_id = this->getDestBankId();
        uint32_t user_id = this->getDestUserId();
        if(!node_id && !user_id) {
            node_id = this->getBankId();
            user_id = this->getUserId();
        }
        Helper::print_user(m_response, ptree, true, node_id, user_id);
        Helper::print_log(ptree, node_id, user_id, m_lastlog, m_txnTypeFilter, getFull());
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
        ptree.put(ERROR_CODE_TAG, m_responseError);
        ptree.put(ERROR_INFO_TAG, m_responseInfo);
    }
}

void GetLog::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.node);
    ptree.put(TAG::SRC_USER, m_data.info.user);
    ptree.put(TAG::FROM, m_data.info.from);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

std::string GetLog::usageHelperToString() {
    std::stringstream ss{};
    ss << "Usage: " << "{\"run\":\"get_log\",[\"from\":<timestamp>],[\"type\":<transaction_type>]}" << "\n";
    ss << "Example: " << "{\"run\":\"get_log\",\"from\":\"1491210824\",\"type\":\"create_account\"}" << "\n";
    return ss.str();
}
