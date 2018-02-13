#ifndef GETACCOUNT_H
#define GETACCOUNT_H

#include "abstraction/interfaces.h"
#include "command/pods.h"


//TODO think about template class wieh data as templete parameter
class GetAccount : public IBlockCommand
{
public:
    GetAccount(uint16_t abank, uint32_t auser, uint16_t bbank, uint16_t buser, uint32_t time);

    //IBlock interface
    virtual int             getType()                                   override;
    virtual unsigned char*  getData()                                   override;
    virtual int             getDataSize()                               override;
    virtual unsigned char*  getSignature()                              override;
    virtual int             getSignatureSize()                          override;
    virtual void            sign(uint8_t* hash,uint8_t* sk,uint8_t* pk) override;
    virtual uint32_t        getUserId()                                 override;
    virtual uint32_t        getBankId()                                 override;

    //IJsonSerialize interface
    virtual std::string                     toString(bool pretty)   override;
    virtual boost::property_tree::ptree     toJson()                override;

private:
    usertxs2 m_data;
};

#endif // GETACCOUNTCOMMAND_H
