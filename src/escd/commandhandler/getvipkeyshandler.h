#ifndef GETVIPKEYSHANDLER_H
#define GETVIPKEYSHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/getvipkeys.h"

class office;

class GetVipKeysHandler : public CommandHandler {
  public:
    GetVipKeysHandler(office& office, client& client);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<GetVipKeys>  m_command;
};

#endif // GETVIPKEYSHANDLER_H
