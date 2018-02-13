#ifndef INTERFACES_H
#define INTERFACES_H

#include <cstdint>
#include <boost/property_tree/json_parser.hpp>

class ICommand
{
public:
    virtual int                     getType()           = 0;
    virtual unsigned char*          getData()           = 0;
    virtual int                     getDataSize()       = 0;
    virtual unsigned char*          getSignature()      = 0;
    virtual int                     getSignatureSize()  = 0;
    virtual void                    sign(uint8_t* hash,uint8_t* sk,uint8_t* pk) = 0;

    virtual uint32_t                getUserId()         = 0;
    virtual uint32_t                getBankId()         = 0;

    virtual ~ICommand() = default;
};


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

#endif // INTERFACES_H
