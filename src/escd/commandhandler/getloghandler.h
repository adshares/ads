#ifndef GETLOGHANDLER_H
#define GETLOGHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/getlog.h"

class office;

class GetLogHandler : public CommandHandler {
  public:
    GetLogHandler(office& office, boost::asio::ip::tcp::socket& socket);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual bool onValidate() override;

  private:
    std::unique_ptr<GetLog>  m_command;
};

#endif // GETLOGHANDLER_H
