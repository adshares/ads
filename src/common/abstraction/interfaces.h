#ifndef INTERFACES_H
#define INTERFACES_H

#include <cstdint>
#include <boost/property_tree/json_parser.hpp>

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


class ICommand
{
public:
    virtual int                     getType()           = 0;
    virtual unsigned char*          getData()           = 0;
    virtual unsigned char*          getResponse()       = 0;
    virtual void                    setData(char* data) = 0;
    virtual void                    setResponse(char* data) = 0;
    virtual int                     getDataSize()       = 0;
    virtual int                     getResponseSize()   = 0;
    virtual unsigned char*          getSignature()      = 0;
    virtual int                     getSignatureSize()  = 0;
    virtual void                    sign(uint8_t* hash, uint8_t* sk, uint8_t* pk)   = 0;
    virtual bool                    checkSignature(uint8_t* hash, uint8_t* pk)      = 0;

    virtual bool                    send(INetworkClient& netCLient) = 0;

    virtual uint32_t                getTime()           = 0;
    virtual uint32_t                getUserId()         = 0;
    virtual uint32_t                getBankId()         = 0;

    virtual ~ICommand() = default;
};

/*class ICommandExecute
{
public:
    virtual void onCommand() = 0;

    virtual ~ICommandExecute() = default;
};*/


//TODO avoid boost in the future. All should based on interfaces.
class IJsonSerialize
{
public:
    virtual boost::property_tree::ptree     toJson()                = 0;
    virtual std::string                     toString(bool preety)   = 0;

    virtual ~IJsonSerialize() = default;
};


class IBlockCommand : public ICommand, public IJsonSerialize
{
};

class BlockCommand: IBlockCommand
{

};

#endif // INTERFACES_H
