#include "factory.h"
#include "pods.h"
#include "getaccount.h"


namespace command{

std::unique_ptr<IBlockCommand> factory::makeCommand(uint8_t type)
{
    std::unique_ptr<IBlockCommand>  command;

    switch(type)
    {
        case TXSTYPE_INF:
            command = std::make_unique<GetAccount>();
    default:
        break;
    }

    return command;
}

}
