#ifndef SETACCOUNTHANDLER_H
#define SETACCOUNTHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/setaccountkey.h"

class office;

class SetAccountKeyHandler : public CommandHandler {
  public:
    SetAccountKeyHandler(office& office, client& client);

    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute()  override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<SetAccountKey>  m_command;
};


#endif // SETACCOUNTHANDLER_H
