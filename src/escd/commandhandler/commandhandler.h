#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"

class office;

class CommandHandler
{
public:
    CommandHandler(office& office, boost::asio::ip::tcp::socket& socket);
    void execute(std::unique_ptr<IBlockCommand> command, user_t usera);

protected:
    virtual void onInit(std::unique_ptr<IBlockCommand> command)  = 0;
    virtual void onExecute()  = 0;
    virtual bool onValidate() = 0;
    //virtual void onCommit(std::unique_ptr<IBlockCommand> command)     = 0;
    //virtual void onSend(std::unique_ptr<IBlockCommand> command)       = 0;

protected:
    office&                         m_offi;
    boost::asio::ip::tcp::socket&   m_socket;
    user_t                          m_usera;
};

#endif // COMMANDHANDLER_H
