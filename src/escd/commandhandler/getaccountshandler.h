#ifndef GETACCOUNTSHANDLER_H
#define GETACCOUNTSHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/getaccounts.h"

class office;

class GetAccountsHandler : public CommandHandler {
  public:
    GetAccountsHandler(office& office, client& client);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<GetAccounts>  m_command;
};

#endif // GETACCOUNTSHANDLER_H
