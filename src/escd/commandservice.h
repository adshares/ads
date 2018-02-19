#ifndef COMMANDSERVICE_H
#define COMMANDSERVICE_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"

class office;

class CommandService
{
public:
    CommandService(office& office, boost::asio::ip::tcp::socket& socket);

    void onExecute(std::unique_ptr<IBlockCommand> command);

private:
    void onGetAccount(std::unique_ptr<IBlockCommand>& command);
    void onSetAccountKey(std::unique_ptr<IBlockCommand>& command);

private:
    office&                         m_offi;
    boost::asio::ip::tcp::socket&   m_socket;
};


#endif // COMMANDSERVICE_H
