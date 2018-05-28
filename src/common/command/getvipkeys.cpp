#include "getvipkeys.h"
#include <sys/stat.h>
#include "helper/hash.h"
#include "helper/json.h"
#include "hash.hpp"

GetVipKeys::GetVipKeys()
    : m_data{} {
    m_responseError = ErrorCodes::Code::eNone;
}

GetVipKeys::GetVipKeys(uint16_t abank, uint32_t auser, uint32_t ttime, uint8_t vhash[32])
    : m_data{abank, auser, ttime, vhash} {
    m_responseError = ErrorCodes::Code::eNone;
}

int GetVipKeys::getType() {
    return TXSTYPE_VIP;
}

unsigned char* GetVipKeys::getData() {
    return reinterpret_cast<unsigned char*>(&m_data.info);
}

unsigned char* GetVipKeys::getResponse() {
    return reinterpret_cast<unsigned char*>(&m_response);
}

void GetVipKeys::setData(char* data) {
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetVipKeys::setResponse(char* response) {
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int GetVipKeys::getDataSize() {
    return sizeof(m_data.info);
}

int GetVipKeys::getResponseSize() {
    return sizeof(m_response);
}

unsigned char* GetVipKeys::getSignature() {
    return m_data.sign;
}

int GetVipKeys::getSignatureSize() {
    return sizeof(m_data.sign);
}

void GetVipKeys::sign(const uint8_t* /*hash*/, const uint8_t* sk, const uint8_t* pk) {
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetVipKeys::checkSignature(const uint8_t* /*hash*/, const uint8_t* pk) {
    return (ed25519_sign_open(getData(), getDataSize(), pk, getSignature()) == 0);
}

user_t& GetVipKeys::getUserInfo() {
    return m_response.usera;
}

uint32_t GetVipKeys::getTime() {
    return m_data.info.ttime;
}

uint32_t GetVipKeys::getUserId() {
    return m_data.info.auser;
}

uint32_t GetVipKeys::getBankId() {
    return m_data.info.abank;
}

int64_t GetVipKeys::getFee() {
    return 0;
}

int64_t GetVipKeys::getDeduct() {
    return 0;
}

bool GetVipKeys::send(INetworkClient& netClient)
{
    if(!netClient.sendData(getData(), sizeof(m_data))) {
        ELOG("GetVipKeys sending error\n");
        return false;
    }

    if(!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
        ELOG("GetVipKeys reading error\n");
        return false;
    }

    if(m_responseError) {
        return true;
    }

    if(!netClient.readData((int32_t*)&m_bufferLength, sizeof(m_bufferLength))) {
        ELOG("GetVipKeys ERROR reading response buffer length\n");
        return false;
    }

    m_responseBuffer = std::make_unique<char[]>(m_bufferLength);
    if(!netClient.readData(m_responseBuffer.get(), m_bufferLength)) {
        ELOG("GetVipKeys ERROR reading response buffer\n");
        return false;
    }

    char hash[65];
    hash[64]='\0';
    ed25519_key2text(hash, getVipHash(), 32);

    if(!checkVipKeys((uint8_t*)m_responseBuffer.get(), m_bufferLength/(2+32))) {
        fprintf(stderr, "ERROR, failed to check VIP keys for hash %s\n", hash);
        return false;
    }

    if(!storeVipKeys(hash)) {
        fprintf(stderr, "ERROR, failed to save VIP keys for hash %s\n", hash);
        return false;
    }

    return true;
}

void GetVipKeys::saveResponse(settings& /*sts*/) {
}

std::string GetVipKeys::toString(bool /*pretty*/) {
    return "";
}

void GetVipKeys::toJson(boost::property_tree::ptree &ptree) {
    char hash[65];
    hash[64]='\0';
    ed25519_key2text(hash, getVipHash(), 32);

    if (!m_responseError) {
        ptree.put("viphash",hash);
        boost::property_tree::ptree viptree;
        for(char* p=m_responseBuffer.get(); p<m_responseBuffer.get()+m_bufferLength; p+=32) {
            ed25519_key2text(hash, (uint8_t*)p, 32);
            boost::property_tree::ptree vipkey;
            vipkey.put("", hash);
            viptree.push_back(std::make_pair("", vipkey));
        }
        ptree.add_child("vipkeys", viptree);
    } else {
        ptree.put(ERROR_TAG, ErrorCodes().getErrorMsg(m_responseError));
    }
}

void GetVipKeys::txnToJson(boost::property_tree::ptree& ptree) {
    using namespace Helper;
    ptree.put(TAG::TYPE, getTxnName(m_data.info.ttype));
    ptree.put(TAG::SRC_NODE, m_data.info.abank);
    ptree.put(TAG::SRC_USER, m_data.info.auser);
    ptree.put(TAG::TIME, m_data.info.ttime);
    ptree.put(TAG::HASH, ed25519_key2text(m_data.info.viphash, 32));
    ptree.put(TAG::SIGN, ed25519_key2text(getSignature(), getSignatureSize()));
}

unsigned char* GetVipKeys::getVipHash() {
    return reinterpret_cast<unsigned char*>(&m_data.info.viphash);
}

bool GetVipKeys::checkVipKeys(uint8_t* vipKeys, int num) {
    hash_t newhash= {};
    hashtree tree(NULL); //FIXME, waste of space

    vipKeys+=2;
    for(int n=0; n<num; n++,vipKeys+=32+2) {
        tree.addhash(newhash, vipKeys);
    }
    return(!memcmp(newhash, getVipHash(), 32));
}

bool GetVipKeys::storeVipKeys(const char* hash) {
    mkdir("vip", 0755);
    char filename[128];
    sprintf(filename, "vip/%64s.vip", hash);

    std::ofstream file(filename, std::ofstream::binary | std::ofstream::out);
    if (file.is_open()) {
        file.write(m_responseBuffer.get(), m_bufferLength);
        fprintf(stderr, "INFO, stored VIP keys in %s\n", filename);
        file.close();
    }
    else {
        fprintf(stderr, "ERROR opening %s\n", filename);
        return false;
    }
    return true;
}
