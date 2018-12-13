#ifndef GETSIGNATURESHANDLER_H
#define GETSIGNATURESHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/getsignatures.h"

class office;

class GetSignaturesHandler : public CommandHandler {
  public:
    GetSignaturesHandler(office& office, client& client);

    virtual void onInit(std::unique_ptr<IBlockCommand> command) override;
    virtual void onExecute()  override;
    virtual void onValidate() override;

  private:
    std::unique_ptr<GetSignatures>  m_command;
};


#endif // GETSIGNATURESHANDLER_H
