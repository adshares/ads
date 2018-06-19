#ifndef CHANGENODEKEYHANDLER_H
#define CHANGENODEKEYHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/changenodekey.h"

class office;

class ChangeNodeKeyHandler : public CommandHandler {
  public:
    ChangeNodeKeyHandler(office& office, boost::asio::ip::tcp::socket& socket);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual ErrorCodes::Code onValidate() override;

  private:
    std::unique_ptr<ChangeNodeKey>  m_command;
};

#endif // CHANGENODEKEYHANDLER_H
