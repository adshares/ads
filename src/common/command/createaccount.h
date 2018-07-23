#ifndef CREATEACCOUNT_H
#define CREATEACCOUNT_H

#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"
#include "errorcodes.h"

class CreateAccount : public BlockCommand {
public:
    CreateAccount();
    CreateAccount(uint16_t src_bank, uint32_t src_user, uint32_t msg_id, uint16_t dst_bank, uint32_t time);

    /** \brief Return TXSTYPE_USR as type . */
    virtual int  getType()                                      override;

    /** \brief Return eModifying as command type . */
    virtual CommandType getCommandType()                        override;

    /** \brief Get pointer to command data structure. */
    virtual unsigned char*  getData()                           override;

    /** \brief Get additional data. Used only on server side to add new created account data */
    virtual unsigned char*          getAdditionalData()         override;

    /** \brief Get pointer to response data. */
    virtual unsigned char*  getResponse()                       override;

    /** \brief Put data as a char list and put convert it to data structure. */
    virtual void setData(char* data)                            override;

    /** \brief Apply data as a response struct. */
    virtual void setResponse(char* response)                    override;

    /** \brief Get data struct size. Without signature. */
    virtual int getDataSize()                                   override;

    /** \brief Get response data struct size. */
    virtual int getResponseSize()                               override;

    /** \brief Get additional data size. */
    virtual int getAdditionalDataSize()                         override;

    /** \brief Get pointer to signature data. */
    virtual unsigned char*  getSignature()                      override;

    /** \brief Get signature size. */
    virtual int getSignatureSize()                              override;

    /** \brief Sign actual data plus hash using user private and public keys.
     *
     * \param hash  Previous hash operation.
     * \param sk    Pointer to provate key.
     * \param pk    Pointer to public key.
    */
    virtual void sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk) override;

    /** \brief Check actual signature.
     *
     * \param hash  Previous hash operation.
     * \param pk    Pointer to public key.
    */
    virtual bool checkSignature(const uint8_t* hash, const uint8_t* pk)  override;

    /** \brief Get time of command. */
    virtual uint32_t        getTime()                                   override;

    /** \brief Get User ID. */
    virtual uint32_t        getUserId()                                 override;

    /** \brief Get Node ID. */
    virtual uint32_t        getBankId()                                 override;

    /** \brief Get command fee. */
    virtual int64_t         getFee()                                    override;

    /** \brief Get change in cash balance after command. */
    virtual int64_t         getDeduct()                                 override;

    /** \brief Send data to the server.
     *
     * \param netClient  Netwrok client implementation of INetworkClient interface.
     * \param pk    Pointer to public key.
    */
    virtual bool            send(INetworkClient& netClient)             override;

    /** \brief Save command response to settings object. */
    virtual void            saveResponse(settings& sts)                 override;

    //IJsonSerialize interface
    virtual std::string  toString(bool pretty)                          override;
    virtual void         toJson(boost::property_tree::ptree &ptree)     override;
    virtual void         txnToJson(boost::property_tree::ptree& ptree)  override;


    /**  \brief Get destination bank id. */
    virtual  uint32_t       getDestBankId();

    /**  \brief Get user message id. */
    virtual  uint32_t       getUserMessageId()                          override;

    /** \brief Put new created account data */
    virtual void setAdditionalData(char* data);

    /**
     * \brief Setting new account data.
     * \param userId - new account user id
     * \param userPkey - new account public key
     */
    virtual void setNewUser(uint32_t userId, uint8_t userPkey[]);

    /** \brief Get public key from additional data. */
    virtual unsigned char*          getPublicKey();
    virtual void                    setPublicKey(uint8_t user_pkey[SHA256_DIGEST_LENGTH]);

private:

    CreateAccountData   m_data;
    commandresponse     m_response;
    NewAccountData      m_newAccount;
};

#endif // CREATEACCOUNT_H
