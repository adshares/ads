#ifndef GETTRANSACTION_H
#define GETTRANSACTION_H

#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"

/** \brief Get transaction command class. */
class GetTransaction : public BlockCommand {
    public:
        GetTransaction();
        GetTransaction(uint16_t src_node, uint32_t src_user, uint16_t dst_node, uint32_t node_msgid, uint32_t position, uint32_t time);

        /** \brief Return TXSTYPE_TXS as type . */
        virtual int  getType()                                      override;

        /** \brief Return eReadingOnly as command type . */
        virtual CommandType getCommandType()                        override;

        /** \brief Get pointer to command data structure. */
        virtual unsigned char*  getData()                           override;

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

        /**  \brief Get message id. */
        virtual uint32_t        getUserMessageId()                          override;

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

        virtual uint16_t    getDestinationNode();
        virtual uint32_t    getNodeMsgId();
        virtual uint16_t    getPosition();

    private:
        virtual bool loadFromLocal();
        virtual bool checkMessageHash(char *data);
        virtual bool saveToFile(char* data);

        std::unique_ptr<IBlockCommand>  m_responseTxn;
        GetTransactionResponse          m_responseHeader;
        std::vector<hash_s>             m_responseHashes;

    public:
        GetTransactionData          m_data;
        commandresponse         m_response;
};

#endif // GETTRANSACTION_H
