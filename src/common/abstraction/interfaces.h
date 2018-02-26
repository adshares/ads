#ifndef INTERFACES_H
#define INTERFACES_H

#include <cstdint>
#include <boost/property_tree/json_parser.hpp>
#include "default.hpp"
#include "settings.hpp"

class office;

class INetworkClient
{
public:
    //INetworkClient(const std::string& address, const std::string& port);
    virtual bool connect()      = 0;
    virtual bool reconnect()    = 0;
    virtual bool disConnect()   = 0;
    virtual bool sendData(uint8_t* buff, int size)      = 0;
    virtual bool sendData(std::vector<uint8_t> buff)    = 0;
    virtual bool readData(uint8_t* buff, int size)      = 0;
    virtual bool readData(char* buff, int size)         = 0;
    virtual bool readData(std::vector<uint8_t>& buff)   = 0;
    virtual ~INetworkClient() = default;
};

/*!
 * \brief Command Interface.
 */
class ICommand
{
public:
    /** \brief Get command type. */
    virtual int                     getType()           = 0;
    /** \brief Get pointer to command data structure. */
    virtual unsigned char*          getData()           = 0;
    /** \brief Get pointer to response data. */
    virtual unsigned char*          getResponse()       = 0;
    /** \brief Put data as a char list and put convert it to data structure. */
    virtual void                    setData(char* data) = 0;
    virtual void                    setResponse(char* data) = 0;
    virtual int                     getDataSize()       = 0;
    virtual int                     getResponseSize()   = 0;
    virtual unsigned char*          getSignature()      = 0;
    virtual int                     getSignatureSize()  = 0;
    virtual void                    sign(uint8_t* hash, uint8_t* sk, uint8_t* pk)   = 0;
    virtual bool                    checkSignature(uint8_t* hash, uint8_t* pk)      = 0;

    virtual bool                    send(INetworkClient& netCLient) = 0;
    virtual user_t&                 getUserInfo()       = 0;
    virtual void                    saveResponse(settings& sts)  = 0;

    virtual uint32_t                getTime()           = 0;
    virtual uint32_t                getUserId()         = 0;
    virtual uint32_t                getBankId()         = 0;
    virtual int64_t                 getFee()            = 0;
    virtual int64_t                 getDeduct()         = 0;

    virtual ~ICommand() = default;
};


//TODO avoid boost in the future. All should based on interfaces.

/*!
 * \brief Interface which allow convert command data to JSON or string.
 */
class IJsonSerialize
{
public:
    virtual boost::property_tree::ptree     toJson()                = 0;
    virtual std::string                     toString(bool preety)   = 0;

    virtual ~IJsonSerialize() = default;
};

/*!
 * \brief Base interface for command. It combain ICommand and IJsonSerialize Interface.
 */
class IBlockCommand : public ICommand, public IJsonSerialize
{
};

class ICommandHandler
{
public:
    virtual void execute(std::unique_ptr<IBlockCommand> command, const user_t& usera) = 0;

    virtual void onInit(std::unique_ptr<IBlockCommand> command)  = 0;
    virtual void onExecute()  = 0;
    virtual bool onValidate() = 0;
    //virtual void onCommit(std::unique_ptr<IBlockCommand> command)     = 0;
    //virtual void onSend()     = 0;

    virtual ~ICommandHandler() = default;
};


#endif // INTERFACES_H
