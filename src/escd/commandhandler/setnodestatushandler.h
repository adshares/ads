#ifndef SETNODESTATUSHANDLER_H
#define SETNODESTATUSHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/setnodestatus.h"

class office;

class SetNodeStatusHandler : public CommandHandler {
  public:
    SetNodeStatusHandler(office& office, boost::asio::ip::tcp::socket& socket);

    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute()  override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<SetNodeStatus>  m_command;
};

#endif // SETNODESTATUSHANDLER_H
