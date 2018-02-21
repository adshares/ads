#ifndef GETACCOUNTHANDLER_H
#define GETACCOUNTHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"

class office;
class GetAccount;

class GetAccountHandler : public CommandHandler
{
public:
    GetAccountHandler(office& office, boost::asio::ip::tcp::socket& socket);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual bool onValidate() override;

private:
    std::unique_ptr<GetAccount>  m_command;
};


#endif // GETACCOUNTHANDLER_H
