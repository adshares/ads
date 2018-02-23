#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"

class office;

class CommandHandler : public ICommandHandler
{
public:
    CommandHandler(office& office, boost::asio::ip::tcp::socket& socket);

    virtual void execute(std::unique_ptr<IBlockCommand> command, const user_t& usera) override;

protected:
    office&                         m_offi;
    boost::asio::ip::tcp::socket&   m_socket;
    user_t                          m_usera;
};

#endif // COMMANDHANDLER_H
