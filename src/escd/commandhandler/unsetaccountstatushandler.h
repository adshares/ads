#ifndef UNSETACCOUNTSTATUSHANDLER_H
#define UNSETACCOUNTSTATUSHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/unsetaccountstatus.h"

class office;

class UnsetAccountStatusHandler : public CommandHandler {
  public:
    UnsetAccountStatusHandler(office& office, client& client);

    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<UnsetAccountStatus> m_command;
};


#endif // UNSETACCOUNTSTATUSHANDLER_H
