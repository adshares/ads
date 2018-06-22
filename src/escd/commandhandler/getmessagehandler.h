#ifndef GETMESSAGEHANDLER_H
#define GETMESSAGEHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/getmessage.h"

class office;

class GetMessageHandler : public CommandHandler {
  public:
    GetMessageHandler(office& office, boost::asio::ip::tcp::socket& socket);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<GetMessage>  m_command;
};


#endif // GETMESSAGEHANDLER_H
