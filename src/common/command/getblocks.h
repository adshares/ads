#ifndef GETBLOCKS_H
#define GETBLOCKS_H

#include <vector>
#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"
#include "errorcodes.h"
#include "helper/vipkeys.h"
#include "helper/signatures.h"
#include "helper/block.h"
#include "helper/hlog.h"

class GetBlocks : public BlockCommand {
    public:
        GetBlocks();
        GetBlocks(uint16_t abank, uint32_t auser, uint32_t ttime, uint32_t from, uint32_t to);

         /** \brief Return TXSTYPE_BLK as type . */
        virtual int getType()                                      override;

        /** \brief Return eReadingOnly as command type . */
        virtual CommandType getCommandType()                       override;

        /** \brief Get pointer to command data structure. */
        virtual unsigned char* getData()                           override;

        /** \brief Get pointer to response data. */
        virtual unsigned char* getResponse()                       override;

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
        virtual uint32_t getUserMessageId()                                 override;

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

        uint32_t getBlockNumberFrom();
        uint32_t getBlockNumberTo();
        void writeStart(uint32_t time);
        bool receiveHeaders(INetworkClient&);
        bool receiveSignatures(INetworkClient&);
        bool receiveFirstVipKeys(INetworkClient& netClient);
        bool receiveNewVipKeys(INetworkClient& netClient);
        bool loadLastHeader();
        bool validateChain();
        bool saveNowhash(const header_t& head);
        bool validateLastBlockUsingFirstKeys();

        uint32_t m_firstKeysLen{};
        VipKeys m_firstVipKeys;
        std::vector<header_t> m_receivedHeaders;
        std::vector<Signature> m_signatures;
        hash_t m_viphash{};
        hash_t m_oldhash{};
        Block m_block;
        GetBlocksData m_data;
        uint32_t m_numOfBlocks{};
        commandresponse m_response;
};

#endif
