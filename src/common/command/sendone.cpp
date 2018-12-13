#include "sendone.h"
#include "ed25519/ed25519.h"
#include "abstraction/interfaces.h"
#include "helper/json.h"
#include "helper/hash.h"

SendOne::SendOne()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

SendOne::SendOne(uint16_t abank, uint32_t auser, uint32_t amsid, uint16_t bbank,
                 uint32_t buser, int64_t tmass, uint8_t tinfo[32], uint32_t time)
    : m_data(abank, auser, amsid, bbank, buser, tmass, time, tinfo) {
    m_responseError = ErrorCodes::Code::eNone;
}

unsigned char* SendOne::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* SendOne::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void SendOne::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void SendOne::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int SendOne::getDataSize() {
    return sizeof(m_data.info);
}

int SendOne::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* SendOne::getSignature() {
    return m_data.sign;
}

int SendOne::getSignatureSize() {
    return sizeof(m_data.sign);
}

int SendOne::getType() {
    return TXSTYPE_PUT;
}

CommandType SendOne::getCommandType() {
    return CommandType::eModifying;
}

void SendOne::sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign2(hash,SHA256_DIGEST_LENGTH , getData(), getDataSize(), sk, pk, getSignature());
}

bool SendOne::checkSignature(const uint8_t* hash, const uint8_t* pk) {
    return (ed25519_sign_open2(hash, SHA256_DIGEST_LENGTH,getData(),getDataSize(),pk,getSignature()) == 0);

}

void SendOne::saveResponse(settings& sts) {
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

uint32_t SendOne::getUserId() {
    return m_data.info.auser;
}

uint32_t SendOne::getBankId() {
    return m_data.info.abank;
}

uint32_t SendOne::getTime() {
    return m_data.info.ttime;
}

unsigned char*  SendOne::getBlockMessage()
{
    return reinterpret_cast<unsigned char*>(&m_data);
}

size_t  SendOne::getBlockMessageSize()
{
    return sizeof(UserSendOne);
}

int64_t SendOne::getFee() {
    int64_t fee=TXS_PUT_FEE(m_data.info.ntmass);
    if(m_data.info.abank!=m_data.info.bbank) {
        fee += TXS_LNG_FEE(m_data.info.ntmass);
    }
    if (fee < TXS_MIN_FEE) {
        fee = TXS_MIN_FEE;
    }
    return fee;
}

int64_t SendOne::getDeduct() {
    return m_data.info.ntmass;
}

bool SendOne::send(INetworkClient& netClient) {
    sendDataSize(netClient);

    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("SendOne sending error\n");
        return false;
    }

    readDataSize(netClient);

    if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("SendOne reading error\n");
        return false;
    }

    if (m_responseError) {
        return true;
    }

    if(!netClient.readData(getResponse(), getResponseSize())) {
        ELOG("SendOne ERROR reading global info\n");
        return false;
    }

    return true;
}

uint32_t SendOne::getDestBankId() {
    return m_data.info.bbank;
}

uint32_t SendOne::getDestUserId() {
    return m_data.info.buser;
}

uint32_t SendOne::getUserMessageId() {
    return m_data.info.namsid;
}

uint8_t* SendOne::getInfoMsg() {
    return m_data.info.ntinfo;
}

std::string SendOne::toString(bool /*pretty*/) {
    return "";
}

void SendOne::toJson(boost::property_tree::ptree& ptree) {
    if (!m_responseError) {
        Helper::print_user(m_response.usera, ptree, true, this->getBankId(), this->getUserId());
        Helper::print_msgid_info(ptree, m_data.info.abank, m_response.msid, m_response.mpos);
    } else {
        if (m_responseError == ErrorCodes::Code::ePkeyDiffers) {
            std::stringstream tx_user_hashin{};
            Helper::ed25519_key2text(tx_user_hashin, m_response.usera.pkey, SHA256_DIGEST_LENGTH);
            ptree.put("tx.account_public_key_new", tx_user_hashin.str());
        }
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void SendOne::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.abank);
    ptree.put(TAG::SRC_USER, m_data.info.auser);
    ptree.put(TAG::MSGID, m_data.info.namsid);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::DST_NODE, m_data.info.bbank);
    ptree.put(TAG::DST_USER, m_data.info.buser);
    ptree.put(TAG::SENDER_FEE, print_amount(this->getFee()));
    ptree.put(TAG::SRC_ADDRESS, Helper::print_address(m_data.info.abank, m_data.info.auser));
    ptree.put(TAG::DST_ADDRESS, Helper::print_address(m_data.info.bbank, m_data.info.buser));
    ptree.put(TAG::AMOUNT, print_amount(m_data.info.ntmass));
    ptree.put(TAG::MSG, Helper::ed25519_key2text(m_data.info.ntinfo, sizeof(m_data.info.ntinfo)));
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

std::string SendOne::usageHelperToString() {
    std::stringstream ss{};
    ss << "Usage: " << "{\"run\":\"send_one\",\"address\":<destination_account_id>,\"amount\":<sending_amount>, [\"message\":\"message_in_hex\"]}" << "\n";
    ss << "Example: " << "{\"run\":\"send_one\",\"address\":\"0003-00000000-XXXX\",\"amount\":2.1,\"message\":\"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F\"}" << "\n";
    return ss.str();
}
