#ifndef SERVICEHANDLER_H
#define SERVICEHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "getaccounthandler.h"
#include "setaccountkeyhandler.h"

class office;

class CommandService
{
public:
    CommandService(office& office, boost::asio::ip::tcp::socket& socket);

    void onExecute(std::unique_ptr<IBlockCommand> command);

private:
    office&                         m_offi;
    boost::asio::ip::tcp::socket&   m_socket;

    GetAccountHandler               m_getAccountHandler;
    SetAccountKeyHandler            m_setAccountHandler;
};


#endif // SERVICEHANDLER_H
