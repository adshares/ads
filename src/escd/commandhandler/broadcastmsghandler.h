#ifndef BORADCASTMSGHANDLER_H
#define BORADCASTMSGHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/broadcastmsg.h"

class office;

class BroadcastMsgHandler : public CommandHandler {
  public:
    BroadcastMsgHandler(office& office, boost::asio::ip::tcp::socket& socket);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual bool onValidate() override;

  private:
    std::unique_ptr<BroadcastMsg>  m_command;
};

#endif // BORADCASTMSGHANDLER_H
