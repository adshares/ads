#ifndef CREATEACCOUNTHANDLER_H
#define CREATEACCOUNTHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/createaccount.h"

class office;

class CreateAccountHandler : public CommandHandler {
public:
  CreateAccountHandler(office& office, boost::asio::ip::tcp::socket& socket);

  //ICommandHandler interface
  virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
  virtual void onExecute() override;
  virtual bool onValidate() override;

private:
  std::unique_ptr<CreateAccount>  m_command;
};

#endif // CREATEACCOUNTHANDLER_H
