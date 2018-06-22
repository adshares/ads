#ifndef GETTRANSACTIONHANDLER_H
#define GETTRANSACTIONHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/gettransaction.h"

class office;

class GetTransactionHandler : public CommandHandler {
  public:
    GetTransactionHandler(office& office, boost::asio::ip::tcp::socket& socket);

    //ICommandHandler interface
    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<GetTransaction>  m_command;
};

#endif // GETTRANSACTIONHANDLER_H
