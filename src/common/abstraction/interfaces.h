#ifndef INTERFACES_H
#define INTERFACES_H

#include <cstdint>
#include <boost/property_tree/json_parser.hpp>
#include "default.hpp"
#include "settings.hpp"
#include "command/errorcodes.h"
#include "command/commandtype.h"

class office;
class client;

/*!
 * \brief Interface for class which is responible for client network connection.
 */
class INetworkClient {
  public:
    /** \brief Connect to server if disconnected. */
    virtual bool connect()      = 0;
    /** \brief Recconect to server. */
    virtual bool reconnect()    = 0;
    /** \brief Disconnect to server. */
    virtual bool disConnect()   = 0;
    /** \brief Check if connected to server. */
    virtual bool isConnected()                          = 0;
    /** \brief Send data using pointer to bufor and size. */
    virtual bool sendData(uint8_t* buff, int size)      = 0;
    /** \brief Send data using vector data. */
    virtual bool sendData(std::vector<uint8_t> buff)    = 0;
    /** \brief Read data to buffor. */
    virtual bool readData(uint8_t* buff, int size)      = 0;
    /** \brief Read data to buffor. */
    virtual bool readData(char* buff, int size)         = 0;
    /** \brief Read data to vector. */
    virtual bool readData(std::vector<uint8_t>& buff)   = 0;
    /** \brief Read data to int. */
    virtual bool readData(int32_t* buff, int size)     = 0;
    virtual ~INetworkClient() = default;
};

/*!
 * \brief Command Interface. Base class for all command.
 */
class ICommand {
  public:
    /** \brief Get type. */
    virtual int                     getType()           = 0;
    /** \brief Get command type. */
    virtual CommandType             getCommandType()    = 0;
    /** \brief Get pointer to command data structure. */
    virtual unsigned char*          getData()           = 0;
    /** \brief Get additional data. */
    virtual unsigned char*          getAdditionalData() = 0;
    /** \brief Get pointer to response data. */
    virtual unsigned char*          getResponse()       = 0;
    /** \brief Put data as a char list and put convert it to data structure. */
    virtual void                    setData(char* data) = 0;
    /** \brief Apply data as a response struct. */
    virtual void                    setResponse(char* data) = 0;
    /** \brief Get data struct size. Without signature. */
    virtual int                     getDataSize()       = 0;
    /** \brief Get response data struct size. */
    virtual int                     getResponseSize()   = 0;
    /** \brief Get additional data size. */
    virtual int                     getAdditionalDataSize() = 0;
    /** \brief Get pointer to signature data. */
    virtual unsigned char*          getSignature()      = 0;
    /** \brief Get signature size. */
    virtual int                     getSignatureSize()  = 0;

    virtual unsigned char* getExtraData() = 0;
    virtual int getExtraDataSize() = 0;
    virtual void setExtraData(std::string data) = 0;

    /** \brief Sign actual data plus hash using user private and public keys.
     *
     * \param hash  Previous hash operation.
     * \param sk    Pointer to provate key.
     * \param pk    Pointer to public key.
    */
    virtual void                    sign(const uint8_t* hash, const uint8_t* sk, const uint8_t* pk)   = 0;
    /** \brief Check actual signature.
     *
     * \param hash  Previous hash operation.
     * \param pk    Pointer to public key.
    */
    virtual bool                    checkSignature(const uint8_t* hash, const uint8_t* pk)      = 0;
    /** \brief Get time of command. */
    virtual uint32_t                getTime()           = 0;
    /** \brief Get User ID. */
    virtual uint32_t                getUserId()         = 0;
    /** \brief Get Node ID. */
    virtual uint32_t                getBankId()         = 0;
    /** \brief Get command fee. */
    virtual int64_t                 getFee()            = 0;
    /** \brief Get change in cash balance after command. */
    virtual int64_t                 getDeduct()         = 0;
    /** \brief Get data as it is stored in blockchain. */
    virtual unsigned char*          getBlockMessage()   = 0;
    /** \brief Get blockchain data size. */
    virtual size_t getBlockMessageSize()                = 0;
    /** \brief Get user message id */
    virtual uint32_t getUserMessageId()                 = 0;
    /** \brief Send data to the server.
     *
     * \param netClient  Netwrok client implementation of INetworkClient interface.
     * \param pk    Pointer to public key.
    */
    virtual bool                    send(INetworkClient& netClient) = 0;
    virtual bool                    sendDataSize(INetworkClient& netClient) = 0;
    virtual bool                    readDataSize(INetworkClient& netClient) = 0;
    /** \brief Save command response to settings object. */
    virtual void                    saveResponse(settings& sts)     = 0;
    virtual void                    setClientVersion(uint32_t version) = 0;
    virtual uint32_t                getClientVersion() = 0;

    virtual ~ICommand() = default;

public:
    ErrorCodes::Code m_responseError;
    uint32_t         m_responseSize;
    std::string      m_responseInfo;
};


//TODO avoid boost in the future. All should based on interfaces.
/*!
 * \brief Interface which allow convert command data to JSON or string. Not used for now.
 */
class IJsonSerialize {
  public:
    virtual void                            toJson(boost::property_tree::ptree& ptree) = 0;
    virtual void                            txnToJson(boost::property_tree::ptree& ptree) = 0;
    virtual std::string                     toString(bool preety)   = 0;
    virtual std::string                     usageHelperToString() = 0;

    virtual ~IJsonSerialize() = default;
};

/*!
 * \brief Base interface for command. It combain ICommand BlockCommandand IJsonSerialize Interface.
 */
class IBlockCommand : public ICommand, public IJsonSerialize {
};

class BlockCommand : public IBlockCommand
{
public:
    unsigned char* getAdditionalData() override { return nullptr; }

    int getAdditionalDataSize() override { return 0; }

    unsigned char* getBlockMessage() override
    {
        return getData();
    }
    /** \brief Get blockchain data size. */
    size_t getBlockMessageSize() override
    {
        return getDataSize() + getSignatureSize() + getAdditionalDataSize();
    }

    unsigned char* getExtraData() override {
        return reinterpret_cast<unsigned char*>(const_cast<char*>(m_extra_data.c_str()));
    }

    int getExtraDataSize() override {
        return m_extra_data.size();
    }

    void setExtraData(std::string data) override {
        m_extra_data = data;
    }


    bool sendDataSize(INetworkClient& netClient) override {
        uint32_t data_size = getDataSize() + getSignatureSize() + getAdditionalDataSize() + getExtraDataSize();
        return netClient.sendData(reinterpret_cast<unsigned char*>(&data_size), sizeof(data_size));
    }

    bool sendData(INetworkClient& netClient) {
        bool res = sendDataSize(netClient);
        res = res && netClient.sendData(getData(), getDataSize());
        if(getAdditionalDataSize() > 0)
            res = res && netClient.sendData(getAdditionalData(), getAdditionalDataSize());
        if(getSignatureSize() > 0)
            res = res && netClient.sendData(getSignature(), getSignatureSize());
        if(getExtraDataSize() > 0)
            res = res && netClient.sendData(getExtraData(), getExtraDataSize());
        return res;
   }

    bool readDataSize(INetworkClient& netClient) override {
        return netClient.readData((int32_t*)&m_responseSize, sizeof(m_responseSize));
    }

    bool readResponseError(INetworkClient& netClient) {
        if (!netClient.readData((int32_t*)&m_responseError, ERROR_CODE_LENGTH)) {
            return false;
        }
        if(m_responseError) {
            ELOG("errorInfo SIZE %d\n", m_responseSize);
            if(m_responseSize > ERROR_CODE_LENGTH) {
                m_responseSize -= ERROR_CODE_LENGTH;


                char errorInfo[4097];
                if(m_responseSize >= sizeof(errorInfo)) {
                    return false;
                }
                if (!netClient.readData(errorInfo, m_responseSize)) {
                    return false;
                }
                errorInfo[m_responseSize] = '\0';
                m_responseInfo = errorInfo;

                if(m_responseError == ErrorCodes::Code::eRedirect) {
    //                char text[13];
    //                text[12] = '\0';
    //                ed25519_key2text(text, (uint8_t*)errorInfo, 6);
    //                std::cerr << text << std::endl;
                    throw RedirectException(errorInfo);
                }
            }
        }
        return true;
    }

    uint32_t getClientVersion() override
    {
        return m_client_version;
    }

    void setClientVersion (uint32_t version) override
    {
        m_client_version = version;
    }

private:
    std::string m_extra_data;
    uint32_t m_client_version;
};

/*!
 * \brief Command handler Interface.
 */
class ICommandHandler {
  public:

    /** \brief Execute command.
     * \param command  Command which dhould be executed.
     * \param usera    User data.
    */
    virtual void execute(std::unique_ptr<IBlockCommand> command, const user_t& usera) = 0;

    /** \brief Int command event. It prepare data's needed to execute command.
     * If "command" casting is needed it should be performed in it.
     *
     * \param command  Unique Pointer with command which should be executed.
    */
    virtual void onInit(std::unique_ptr<IBlockCommand> command)  = 0;

    /** \brief Execution event. It executes actual command and commit it to blockchain. */
    virtual void onExecute()  = 0;

    /** \brief Validation event. It performs command specific validation*/
    virtual void onValidate() = 0;

    virtual ~ICommandHandler() = default;
};


#endif // INTERFACES_H
