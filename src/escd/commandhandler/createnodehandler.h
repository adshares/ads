#ifndef CREATENODEHANDLER_H
#define CREATENODEHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/createnode.h"

class office;

class CreateNodeHandler : public CommandHandler {
  public:
    CreateNodeHandler(office& office, client& client);

    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute()  override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<CreateNode> m_command;
};


#endif // CREATENODEHANDLER_H
