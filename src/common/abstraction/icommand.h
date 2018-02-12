#ifndef ICOMMAND_H
#define ICOMMAND_H


class IBlock
{
public:
    virtual int                     getType()                                   = 0;
    virtual unsigned char*          getData()                                   = 0;
    virtual unsigned char*          getSignature()                              = 0;
    virtual int                     getSignatureSize()                          = 0;
    virtual int                     getDataSize()                               = 0;
    virtual void                    sign(uint8_t* hash,uint8_t* sk,uint8_t* pk) = 0;

    virtual uint32_t                getUserId()     = 0;
    virtual uint32_t                getBankId()     = 0;

    virtual ~IBlock() = default;
};

#endif // ICOMMAND_H
