#ifndef GETACCOUNTHANDLER_H
#define GETACCOUNTHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/getaccount.h"

class office;

class GetAccountHandler : public CommandHandler {
  public:
    GetAccountHandler(office& office, client& client);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<GetAccount>  m_command;
};


#endif // GETACCOUNTHANDLER_H
