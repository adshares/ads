#include "getaccounthandler.h"
#include "command/getaccount.h"
#include "../office.hpp"

GetAccountHandler::GetAccountHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket)
{
}

void GetAccountHandler::onInit(std::unique_ptr<IBlockCommand> command)
{    
    try
    {
        m_command = std::unique_ptr<GetAccount>(dynamic_cast<GetAccount*>(command.release()));
    }
    catch (std::bad_cast& bc)
    {
        DLOG("OnGetAccount bad_cast caught: %s", bc.what());
        return;
    }
}

void GetAccountHandler::onExecute()
{
    assert(m_command);

    userinfo&   data    = m_command->getDataStruct()._info;

    user_t userb;

    if(data._abank != data._bbank || data._auser != data._buser)
    {
        if(!m_offi.get_user(userb, data._bbank, data._buser))
        {
            DLOG("FAILED to get user info %08X:%04X\n", data._bbank, data._buser);
            return;
        }
        boost::asio::write(m_socket, boost::asio::buffer(&userb, sizeof(user_t)));
    }
    else
    {
        boost::asio::write(m_socket,boost::asio::buffer(&m_usera, sizeof(user_t)));
    }

    //TODO Check why two times we are sending data.
    if(data._bbank)
    {
        if(!m_offi.get_user_global(userb, data._bbank, data._buser))
        {
            DLOG("FAILED to get global user info %08X:%04X\n", data._bbank, data._buser);
            return;
        }
        boost::asio::write(m_socket,boost::asio::buffer(&userb, sizeof(user_t)));
    }
}

bool GetAccountHandler::onValidate()
{
    userinfo&   data    = m_command->getDataStruct()._info;
    int32_t     diff    = data._ttime - time(nullptr);

    // this is special, just local info
    if((abs(diff)>22))
    {
        DLOG("ERROR: high time difference (%d>2s)\n",diff);
        return false;
    }

//FIXME, read data also from server
//FIXME, if local account locked, check if unlock was successfull based on time passed after change
    if(data._abank != m_offi.svid && data._bbank != m_offi.svid)
    {
        DLOG("ERROR: bad bank for INF abank: %d bbank: %d SVID: %d\n", data._abank, data._bbank, m_offi.svid );
        return false;
    }

#ifdef DEBUG
  DLOG("SENDING user info %08X:%04X\n", data._bbank, data._buser);
#endif

    return true;
}