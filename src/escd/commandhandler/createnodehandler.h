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
    CreateNodeHandler(office& office, boost::asio::ip::tcp::socket& socket);

    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute()  override;
    virtual ErrorCodes::Code onValidate() override;

  private:
    std::unique_ptr<CreateNode> m_command;
};


#endif // CREATENODEHANDLER_H
