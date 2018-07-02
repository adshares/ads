#ifndef RETRIEVEFUNDSHANDLER_H
#define RETRIEVEFUNDSHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/retrievefunds.h"

class office;

class RetrieveFundsHandler : public CommandHandler {
  public:
    RetrieveFundsHandler(office& office, boost::asio::ip::tcp::socket& socket);

    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<RetrieveFunds> m_command;
};


#endif // RETRIEVEFUNDSHANDLER_H
