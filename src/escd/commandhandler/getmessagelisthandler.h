#ifndef GETMESSAGELISTHANDLER_H
#define GETMESSAGELISTHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/getmessagelist.h"

class office;

class GetMessageListHandler : public CommandHandler {
  public:
    GetMessageListHandler(office& office, boost::asio::ip::tcp::socket& socket);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<GetMessageList>  m_command;
};


#endif // GETMESSAGELISTHANDLER_H
