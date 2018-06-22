#ifndef SENDMANYHANDLER_H
#define SENDMANYHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/sendmany.h"

class office;

class SendManyHandler : public CommandHandler {
  public:
    SendManyHandler(office& office, boost::asio::ip::tcp::socket& socket);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<SendMany> m_command;
};

#endif // SENDMANYHANDLER_H
