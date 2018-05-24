#ifndef BROADCASTMSG_H
#define BROADCASTMSG_H

#define MAX_BROADCAST_LENGTH  32000 //64k in hex multiplied by 2

#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"

/** \brief Broadcast command class */
class BroadcastMsg : public IBlockCommand {
    public:
        BroadcastMsg();
        BroadcastMsg(uint16_t src_bank, uint32_t src_user, uint32_t msg_id, uint16_t msg_length, const char* msg, uint32_t time);

        /** \brief Disabled copy constructor. */
        BroadcastMsg(const BroadcastMsg& obj) = delete;

        /** \brief Disable copy assignment operator. */
        BroadcastMsg &operator=(const BroadcastMsg&) = delete;

        /** \brief Free completeData object resources. */
        virtual ~BroadcastMsg();

        /** \brief Return TXSTYPE_BRO as command type . */
        virtual int  getType()                                      override;

        /** \brief Get pointer to command data structure. */
        virtual unsigned char*  getData()                           override;

        /** \brief Get additional data. */
        virtual unsigned char*  getAdditionalData()                 override;

        /** \brief Get pointer to response data. */
        virtual unsigned char*  getResponse()                       override;

        /** \brief Put data as a char list and put convert it to data structure. */
        virtual void setData(char* data)                            override;

        /** \brief Apply data as a response struct. */
        virtual void setResponse(char* response)                    override;

        /** \brief Get data struct size. Without signature. */
        virtual int getDataSize()                                   override;

        /** \brief Get additional data size. */
        virtual int getAdditionalDataSize()                         override;

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

        /** \brief Get actual blockchain user info. */
        virtual user_t&         getUserInfo()                               override;

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
        virtual void         toJson(boost::property_tree::ptree& ptree)     override;
        virtual void         txnToJson(boost::property_tree::ptree& ptree)  override;

      public:

        /**  \brief Get message id. */
        virtual  uint32_t       getUserMessageId();

        SendBroadcastData   m_data;
        unsigned char*      m_message;
        commandresponse     m_response;
};


#endif // BROADCASTMSG_H
