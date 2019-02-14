#ifndef LOGACCOUNTHANDLER_H
#define LOGACCOUNTHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/logaccount.h"

class office;

class LogAccountHandler : public CommandHandler {
  public:
    LogAccountHandler(office& office, client& client);

    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute()  override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<LogAccount>  m_command;
};


#endif
