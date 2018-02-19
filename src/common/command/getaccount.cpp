#include "getaccount.h"
#include "../ed25519/ed25519.h"
#include "abstraction/interfaces.h"

GetAccount::GetAccount()
    : m_data{}
{
}

GetAccount::GetAccount(uint16_t abank, uint32_t auser, uint16_t bbank, uint16_t buser,
                       uint32_t time)
    : m_data( abank, auser, bbank, buser, time)
{
}

//IBlock interface
unsigned char* GetAccount::getData()
{
    return reinterpret_cast<unsigned char*>(&m_data._info);
}

unsigned char* GetAccount::getResponse()
{
    return reinterpret_cast<unsigned char*>(&m_response);
}

void GetAccount::setData(char* data)
{
    m_data = *reinterpret_cast<decltype(m_data)*>(data);
}

void GetAccount::setResponse(char* response)
{
    m_response = *reinterpret_cast<decltype(m_response)*>(response);
}

int GetAccount::getDataSize()
{
    return sizeof(m_data._info);
}

int GetAccount::getResponseSize()
{
    return 2*sizeof(decltype(m_response));
}

unsigned char* GetAccount::getSignature()
{
    return m_data._sign;
}

int GetAccount::getSignatureSize()
{
    return sizeof(m_data._sign);
}

int GetAccount::getType()
{
    return m_data._info._ttype;
}

void GetAccount::sign(uint8_t* hash, uint8_t* sk, uint8_t* pk)
{
    ed25519_sign(getData(), getDataSize(), sk, pk, getSignature());
}

bool GetAccount::checkSignature(uint8_t* hash, uint8_t* pk)
{
    return( ed25519_sign_open( getData() , getDataSize() , pk, getSignature() ) == 1);
}

uint32_t GetAccount::getUserId()
{
    return m_data._info._auser;
}

uint32_t GetAccount::getBankId()
{
    return m_data._info._abank;
}

uint32_t GetAccount::getTime()
{
    return m_data._info._ttime;
}

bool GetAccount::send(INetworkClient& netClient)
{
    if(! netClient.sendData(getData(), getDataSize() + getSignatureSize() )){
        return false;
    }

    if(!netClient.readData(getResponse(), getResponseSize())){
        std::cerr<<"GetAccount ERROR reading global info\n";
        return false;
    }

    return true;
}

std::string GetAccount::toString(bool pretty)
{
    return "";
}

boost::property_tree::ptree GetAccount::toJson()
{
    return boost::property_tree::ptree();
}
