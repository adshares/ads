#ifndef GETLOG_H
#define GETLOG_H

#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"
#include "settings.hpp"
#include "errorcodes.h"

/*!
 * \brief Class responsible for handling "get_log" command.
 */
class GetLog : public BlockCommand {
  public:
    GetLog();
    /** \brief
     * \param bank - node
     * \param user - user
     * \param from - collect logs from blog timestamp
     * \param txnTypeFilter - text name of transaction to user filter eg. send_one, create_account, "" for all
     */
    GetLog(uint16_t bank, uint32_t user, uint32_t from, uint16_t dst_node, uint32_t dst_user, bool full, const char* txnTypeFilter);

    //IBlock interface
    /** \brief Return TXSTYPE_LOG as type . */
    virtual int  getType()                                      override;

    /** \brief Return eReadingOnly as command type . */
    virtual CommandType getCommandType()                        override;

    /** \brief Get pointer to command data structure. */
    virtual unsigned char*  getData()                           override;

    /** \brief Get pointer to response data. */
    virtual unsigned char*  getResponse()                       override;

    /**  \brief Get message id. */
    virtual uint32_t getUserMessageId()                         override;

    /** \brief Put data as a char list and put convert it to data structure. */
    virtual void setData(char* data)                            override;

    /** \brief Apply data as a response struct. */
    virtual void setResponse(char* response)                    override;

    /** \brief Get data struct size. Without signature. */
    virtual int getDataSize()                                   override;

    /** \brief Get response data struct size. */
    virtual int getResponseSize()                               override;

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

    /**  \brief Get destination bank id. */
    virtual  uint16_t       getDestBankId();

            /**  \brief Get destination user id. */
    virtual  uint32_t       getDestUserId();

    /** \brief Get full log. */
    virtual uint8_t         getFull();

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
    virtual std::string  usageHelperToString()                          override;

    /** \brief Reads last logs update time */
    virtual void getLastLogsUpdate();

  private:
    GetLogData          m_data;
    user_t              m_response;
    uint32_t            m_lastlog;
    int                 m_txnTypeFilter;
};


#endif // GETLOG_H
