#include "getaccount.h"
#include "../ed25519/ed25519.h"

GetAccount::GetAccount(uint16_t abank, uint32_t auser, uint16_t bbank, uint16_t buser, uint32_t time)
    :m_data( abank, auser, bbank, buser, time)
{
}

//IBlock interface
unsigned char* GetAccount::getData()
{
    return reinterpret_cast<unsigned char*>(&m_data._info);
}

int GetAccount::getDataSize()
{
    return sizeof(m_data._info);
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

uint32_t GetAccount::getUserId()
{
    return m_data._info._buser;
}

uint32_t GetAccount::getBankId()
{
    return m_data._info._bbank;
}


std::string GetAccount::toString(bool pretty)
{
    return "";
}

boost::property_tree::ptree GetAccount::toJson()
{
    return boost::property_tree::ptree();
}
