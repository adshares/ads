#ifndef TRANSACTIONACCEPTED_H
#define TRANSACTIONACCEPTED_H

#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"
#include "settings.hpp"
#include "errorcodes.h"

/*!
 * \brief Transaction accepted TXSTYPE_UOK type
 */
class TransactionAccepted : public BlockCommand {
  public:
    TransactionAccepted();
    TransactionAccepted(uint16_t abank, uint32_t auser, uint32_t amsid, uint32_t ttime, uint16_t bbank, uint32_t buser, uint8_t public_key[32]);

    //IBlock interface
    virtual int  getType()                                      override;
    virtual CommandType getCommandType()                        override;
    virtual unsigned char*  getData()                           override;
    virtual unsigned char*  getResponse()                       override;
    virtual void setData(char* data)                            override;
    virtual void setResponse(char* response)                    override;
    virtual int getDataSize()                                   override;
    virtual int getResponseSize()                               override;
    virtual unsigned char*  getSignature()                      override;
    virtual int getSignatureSize()                              override;
    virtual void sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) override;
    virtual bool checkSignature(const uint8_t* hash, const uint8_t* pk)  override;
    virtual uint32_t        getTime()                                   override;
    virtual uint32_t        getUserId()                                 override;
    virtual uint32_t        getBankId()                                 override;
    virtual int64_t         getFee()                                    override;
    virtual int64_t         getDeduct()                                 override;
    virtual uint32_t        getUserMessageId()                          override;
    virtual bool            send(INetworkClient& netClient)             override;
    virtual void            saveResponse(settings& sts)                 override;

    //IJsonSerialize interface
    virtual std::string  toString(bool pretty)                          override;
    virtual void         toJson(boost::property_tree::ptree &ptree)     override;
    virtual void         txnToJson(boost::property_tree::ptree& ptree)  override;

   private:
    TransactionAcceptedInfo    m_data;
    user_t                     m_response;
};

#endif // TRANSACTIONACCEPTED_H
