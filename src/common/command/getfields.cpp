#include "getfields.h"

#include "factory.h"
#include "helper/txsname.h"

GetFields::GetFields() : m_type(TXSTYPE_MAX){
}

GetFields::GetFields(const char* txnType) {
    m_type = Helper::getTxnTypeId(txnType);
}

unsigned char* GetFields::getData() {
    return reinterpret_cast<unsigned char*>(&m_data);
}

unsigned char* GetFields::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void GetFields::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetFields::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int GetFields::getDataSize() {
    return sizeof(m_data.info);
}

int GetFields::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* GetFields::getSignature() {
    return m_data.sign;
}

int GetFields::getSignatureSize() {
    return sizeof(m_data.sign);
}

int GetFields::getType() {
    return TXSTYPE_GFI;
}

CommandType GetFields::getCommandType() {
    return CommandType::eReadingOnly;
}

void GetFields::sign(const uint8_t* /*hash*/, const uint8_t* /*sk*/, const uint8_t* /*pk*/) {
}

bool GetFields::checkSignature(const uint8_t* /*hash*/, const uint8_t* /*pk*/) {
    return true;
}

void GetFields::saveResponse(settings& /*sts*/) {
}

uint32_t GetFields::getUserId() {
    return 0;
}

uint32_t GetFields::getBankId() {
    return 0;
}

uint32_t GetFields::getTime() {
    return 0;
}

int64_t GetFields::getFee() {
    return 0;
}

int64_t GetFields::getDeduct() {
    return 0;
}

uint32_t GetFields::getUserMessageId() {
    return 0;
}

bool GetFields::send(INetworkClient& /*netClient*/) {
    return true;
}

std::string GetFields::toString(bool /*pretty*/) {
    return "";
}

void GetFields::toJson(boost::property_tree::ptree& ptree) {
    ptree.clear();
    std::unique_ptr<IBlockCommand> command;
    command = command::factory::makeCommand(m_type);
    if (command) {
        command->txnToJson(ptree);
        boost::property_tree::ptree result;
        for (auto it : ptree) {
            boost::property_tree::ptree child;
            child.put("", it.first.data());
            result.push_back(std::make_pair("", child));
        }
        ptree.clear();
        ptree.add_child(Helper::getTxnName(m_type), result);
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(ErrorCodes::Code::eIncorrectTransaction));
    }
}

void GetFields::txnToJson(boost::property_tree::ptree& /*ptree*/) {
}
