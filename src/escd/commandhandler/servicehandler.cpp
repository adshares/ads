#include "servicehandler.h"
#include "command/getaccount.h"
#include "command/setaccountkey.h"
#include "../office.hpp"

CommandService::CommandService(office& office, boost::asio::ip::tcp::socket& socket)
    : m_offi(office),
      m_socket(socket),
      m_getAccountHandler(office, socket),
      m_setAccountHandler(office, socket)
{
}

void CommandService::onExecute(std::unique_ptr<IBlockCommand> command)
{        
    user_t usera;
    if(!m_offi.get_user(usera, command->getBankId(), command->getUserId()))
    {
        DLOG("ERROR: read user failed\n");
        return;
    };

    switch(command->getType())
    {
        case TXSTYPE_INF:
            m_getAccountHandler.execute(std::move(command), std::move(usera));
            break;
        case TXSTYPE_KEY:
            m_setAccountHandler.execute(std::move(command), std::move(usera));
            break;
        default:
            break;
    }
}
