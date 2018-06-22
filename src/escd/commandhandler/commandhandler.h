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
    struct CommitResult {
        ErrorCodes::Code errorCode;
        uint32_t msid;
        uint32_t mpos;
    };
    CommitResult commitChanges(IBlockCommand& command);

    template<typename CommandType>
    std::unique_ptr<CommandType> init(std::unique_ptr<IBlockCommand> command);

  private:
    void executeImpl(std::unique_ptr<IBlockCommand>);
    void performCommonValidation(IBlockCommand&);
    void validateModifyingCommand(IBlockCommand&);
    void sendErrorToClient(ErrorCodes::Code);
};

template<typename CommandType>
std::unique_ptr<CommandType> CommandHandler::init(std::unique_ptr<IBlockCommand> cmd) {
    if(CommandType* result = dynamic_cast<CommandType*>(cmd.get())) {
        cmd.release();
        return std::unique_ptr<CommandType>(result);
    }
    ELOG("ERROR while downcasting\n");
    return std::unique_ptr<CommandType>(nullptr);
}

#endif // COMMANDHANDLER_H
