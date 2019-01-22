#include "getsignatures.h"
#include "helper/hash.h"
#include "helper/json.h"
#include "helper/signatures.h"

GetSignatures::GetSignatures()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

GetSignatures::GetSignatures(uint16_t abank, uint32_t auser, uint32_t ttime, uint32_t block)
    : m_data{abank, auser, ttime, block} {
    m_responseError = ErrorCodes::Code::eNone;
}

int GetSignatures::getType() {
    return TXSTYPE_SIG;
}

CommandType GetSignatures::getCommandType() {
    return CommandType::eReadingOnly;
}

unsigned char* GetSignatures::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetSignatures::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void GetSignatures::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetSignatures::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int GetSignatures::getDataSize() {
    return sizeof(m_data.info);
}

int GetSignatures::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* GetSignatures::getSignature() {
    return m_data.sign;
}

int GetSignatures::getSignatureSize() {
    return sizeof(m_data.sign);
}

void GetSignatures::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetSignatures::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return (ed25519_sign_open(getData(), getDataSize(), pk, getSignature()) == 0);
}

uint32_t GetSignatures::getTime() {
    return m_data.info.ttime;
}

uint32_t GetSignatures::getUserId() {
    return m_data.info.auser;
}

uint32_t GetSignatures::getBankId() {
    return m_data.info.abank;
}

int64_t GetSignatures::getFee() {
    return 0;
}

int64_t GetSignatures::getDeduct() {
    return 0;
}

uint32_t GetSignatures::getUserMessageId() {
    return 0;
}

bool GetSignatures::send(INetworkClient& netClient)
{
    sendDataSize(netClient);

    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("GetSignatures sending error\n");
        return false;
    }

    readDataSize(netClient);

    if(!readResponseError(netClient)) {
        ELOG("GetSignatures reading error\n");
        return false;
    }

    if(m_responseError) {
        return true;
    }

    int32_t signaturesOkSize=0, signaturesNoSize=0;

    if(!netClient.readData((int32_t*)&signaturesOkSize, sizeof(int32_t))) {
        ELOG("GetSignatures reading error\n");
        return false;
    }
    for(int i=0; i<signaturesOkSize; ++i) {
        Helper::Signature sign;
        if(!netClient.readData((char*)&sign, sizeof(sign))) {
            ELOG("GetSignatures reading error\n");
            return false;
        }
        m_signaturesOk.push_back(sign);
    }

    if(!netClient.readData((int32_t*)&signaturesNoSize, sizeof(int32_t))) {
        ELOG("GetSignatures reading error\n");
        return false;
    }
    for(int i=0; i<signaturesNoSize; ++i) {
        Helper::Signature sign;
        if(!netClient.readData((char*)&sign, sizeof(sign))) {
            ELOG("GetSignatures reading error\n");
            return false;
        }
        m_signaturesNo.push_back(sign);
    }

    return true;
}

void GetSignatures::saveResponse(settings&/* sts*/) {
}

std::string GetSignatures::toString(bool /*pretty*/) {
    return "";
}

boost::property_tree::ptree prepareTree(const std::vector<Helper::Signature>& signatures) {
    char sigh[129];
    sigh[128]='\0';

    boost::property_tree::ptree tree;
    for(const auto& sig : signatures) {
        ed25519_key2text(sigh, &sig.signature[0], 64);
        boost::property_tree::ptree sigTree;
        sigTree.put("node", sig.svid);
        sigTree.put("signature", sigh);
        tree.push_back(std::make_pair("", sigTree));
    }
    return tree;
}

void GetSignatures::toJson(boost::property_tree::ptree &ptree) {
    if(!m_responseError) {

        char blockhex[9];
        blockhex[8]='\0';
        sprintf(blockhex,"%08X", getBlockNumber());
        ptree.put("block_time_hex",blockhex);
        ptree.put("block_time", getBlockNumber());

        if(m_signaturesOk.size() > 0) {
            ptree.add_child("signatures", prepareTree(m_signaturesOk));
        }

        if(m_signaturesNo.size() > 0) {
            ptree.add_child("fork_signatures", prepareTree(m_signaturesNo));
        }
    }
    else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
        ptree.put(ERROR_CODE_TAG, m_responseError);
        ptree.put(ERROR_INFO_TAG, m_responseInfo);
    }
}

void GetSignatures::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.abank);
    ptree.put(TAG::SRC_USER, m_data.info.auser);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::BLOCK, m_data.info.block);
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

uint32_t GetSignatures::getBlockNumber() {
    return m_data.info.block;
}

std::string GetSignatures::usageHelperToString() {
    std::stringstream ss{};
    ss << "Usage: " << "{\"run\":\"get_signatures\",[\"block\":<block time as hex>]}" << "\n";
    ss << "Example: " << "{\"run\":\"get_signatures\",\"block\":\"5B1A48C0\"}" << "\n";
    return ss.str();
}
