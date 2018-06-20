#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "command/pods.h"
#include "default.hpp"

class office;

/*!
 * \brief Base class for all command handlers.
 */
class CommandHandler : public ICommandHandler {
  public:
    /** \brief Constructor.
      * \param office  Office object.
      * \param socket  Socket connected with client. */
    CommandHandler(office& office, boost::asio::ip::tcp::socket& socket);
    virtual void execute(std::unique_ptr<IBlockCommand> command, const user_t& usera) override;
  protected:
    office&                         m_offi;     ///< Reference to office object.
    boost::asio::ip::tcp::socket&   m_socket;   ///< Socket for connection with client.
    user_t                          m_usera;    ///< Current user blockchain data.
  private:
    ErrorCodes::Code executeImpl(std::unique_ptr<IBlockCommand>);
    ErrorCodes::Code performCommonValidation(IBlockCommand&);
    ErrorCodes::Code validateModifyingCommand(IBlockCommand&);
    ErrorCodes::Code initialValidation(IBlockCommand&);
};

#endif // COMMANDHANDLER_H
