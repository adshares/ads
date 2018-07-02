#ifndef SENDONEHANDLER_H
#define SENDONEHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/sendone.h"

class office;

class SendOneHandler : public CommandHandler {
  public:
    SendOneHandler(office& office, boost::asio::ip::tcp::socket& socket);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<SendOne>  m_command;
};

#endif // SENDONEHANDLER_H
