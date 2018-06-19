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
    UnsetAccountStatusHandler(office& office, boost::asio::ip::tcp::socket& socket);

    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual ErrorCodes::Code onValidate() override;

  private:
    ErrorCodes::Code validate();
    ErrorCodes::Code performCommandSpecificValidation();
    std::unique_ptr<UnsetAccountStatus> m_command;
};


#endif // UNSETACCOUNTSTATUSHANDLER_H
