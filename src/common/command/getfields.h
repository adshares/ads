#ifndef GETFIELDS_H
#define GETFIELDS_H

#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"
#include "settings.hpp"
#include "errorcodes.h"

/*!
 * \brief Readonly class (without connection to daemon, to obtain data fields of certain command typed as argument
 */
class GetFields : public IBlockCommand {
  public:
    GetFields();
    GetFields(const char* txnType);

    //IBlock interface
    virtual int  getType()                                      override;
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
    virtual user_t&         getUserInfo()                               override;
    virtual uint32_t        getTime()                                   override;
    virtual uint32_t        getUserId()                                 override;
    virtual uint32_t        getBankId()                                 override;
    virtual int64_t         getFee()                                    override;
    virtual int64_t         getDeduct()                                 override;
    virtual bool            send(INetworkClient& netClient)             override;
    virtual void            saveResponse(settings& sts)                 override;

    //IJsonSerialize interface
    virtual std::string  toString(bool pretty)                          override;
    virtual void         toJson(boost::property_tree::ptree &ptree)     override;
    virtual void         txnToJson(boost::property_tree::ptree& ptree)  override;

   private:
    usertxs2    m_data;
    user_t      m_response;
    uint8_t     m_type;
};

#endif // GETFIELDS_H
