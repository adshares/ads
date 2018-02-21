#ifndef SETACCOUNTKEY_H
#define SETACCOUNTKEY_H

#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"

//TODO think about template class wieh data as templete parameter
class SetAccountKey : public IBlockCommand
{
public:
    SetAccountKey();
    SetAccountKey(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t time, uint8_t pubkey[32], uint8_t pubkeysign[64]);

    bool            checkPubKeySignaure();

    //IBlock interface
    virtual int             getType()                                   override;
    virtual unsigned char*  getData()                                   override;
    virtual unsigned char*  getResponse()                               override;
    virtual void            setData(char* data)                         override;
    virtual void            setResponse(char* response)                 override;
    virtual int             getDataSize()                               override;
    virtual int             getResponseSize()                           override;
    virtual unsigned char*  getSignature()                              override;
    virtual int             getSignatureSize()                          override;
    virtual void            sign(uint8_t* hash,uint8_t* sk,uint8_t* pk) override;
    virtual bool            checkSignature(uint8_t* hash, uint8_t* pk)  override;
    virtual uint32_t        getUserId()                                 override;
    virtual uint32_t        getBankId()                                 override;
    virtual uint32_t        getTime()                                   override;

    virtual bool            send(INetworkClient& netClient)             override;

    //IJsonSerialize interface
    virtual std::string                     toString(bool pretty)   override;
    virtual boost::property_tree::ptree     toJson()                override;

    accountkey& getDataStruct() { return m_data;}

private:
    accountkey          m_data;
    commandresponse     m_response;
};

#endif // SETACCOUNTKEY_H
