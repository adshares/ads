#include "setaccountkey.h"
#include "../ed25519/ed25519.h"
#include "abstraction/interfaces.h"

SetAccountKey::SetAccountKey()
    : m_data{}
{
}

SetAccountKey::SetAccountKey(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t time, uint8_t pubkey[32], uint8_t pubkeysign[64])
    : m_data( abank, auser, amsid, time, pubkey, pubkeysign)
{
}

bool SetAccountKey::checkPubKeySignaure()
{
    return(ed25519_sign_open((uint8_t*)nullptr, 0, m_data._pubkey , m_data._pubkeysign) != -1);
}

//IBlock interface
unsigned char* SetAccountKey::getData()
{
    return reinterpret_cast<unsigned char*>(&m_data);
}

unsigned char* SetAccountKey::getResponse()
{
    return reinterpret_cast<unsigned char*>(&m_response);
}

void SetAccountKey::setData(char* data)
{
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void SetAccountKey::setResponse(char* response)
{
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int SetAccountKey::getDataSize()
{
    return sizeof(m_data._ttype) + sizeof(m_data._abank) + sizeof(m_data._auser) + sizeof(m_data._amsid) + sizeof(m_data._ttime) + sizeof(m_data._pubkey);
}

int SetAccountKey::getResponseSize()
{
    return sizeof(user_t);
}

unsigned char* SetAccountKey::getSignature()
{
    return m_data._sign;
}

int SetAccountKey::getSignatureSize()
{
    return sizeof(m_data._sign);
}

int SetAccountKey::getType()
{
    return m_data._ttype;
}

void SetAccountKey::sign(uint8_t* hash, uint8_t* sk, uint8_t* pk)
{
    ed25519_sign2(hash, 32, getData(), getDataSize(), sk, pk, getSignature());
}

bool SetAccountKey::checkSignature(uint8_t* hash, uint8_t* pk)
{    
    return( ed25519_sign_open2( hash , 32 , getData(), getDataSize(), pk, getSignature() ) == 0);
}

uint32_t SetAccountKey::getUserId()
{
    return m_data._auser;
}

uint32_t SetAccountKey::getBankId()
{
    return m_data._abank;
}

uint32_t SetAccountKey::getTime()
{
    return m_data._ttime;
}

int64_t SetAccountKey::getFee()
{
    return TXS_KEY_FEE;
}

int64_t SetAccountKey::getDeduct()
{
    return 0;
}

bool SetAccountKey::send(INetworkClient& netClient)
{
    if(! netClient.sendData(getData(), sizeof(m_data) )){
        return false;
    }

    if(!netClient.readData(getResponse(), getResponseSize())){
        std::cerr<<"SetAccountKey ERROR reading global info\n";
        return false;
    }        

    return true;
}

std::string SetAccountKey::toString(bool pretty)
{
    return "";
}

boost::property_tree::ptree SetAccountKey::toJson()
{
    return boost::property_tree::ptree();
}
