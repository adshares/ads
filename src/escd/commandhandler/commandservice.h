#ifndef SERVICEHANDLER_H
#define SERVICEHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "getaccounthandler.h"
#include "setaccountkeyhandler.h"
#include "createnodehandler.h"
#include "sendonehandler.h"

class office;

/*!
 * \brief Command service. It execute proper action for commnads.
 */
class CommandService {
  public:
    /** \brief Constructor.
      * \param office  Office object.
      * \param socket  Socket connected with client. */
    CommandService(office& office, boost::asio::ip::tcp::socket& socket);

    /** \brief execute method which invokes command handler. */
    void onExecute(std::unique_ptr<IBlockCommand> command);

  private:
    office&                m_offi;              ///< reference to office object.
    GetAccountHandler      m_getAccountHandler; ///< get account and get me command object handler.
    SetAccountKeyHandler   m_setAccountHandler; ///< set account key command object handler.
    CreateNodeHandler	   m_createNodeHandler; ///< create node command object handler.
    SendOneHandler         m_sendOneHandler;    ///< send one command handler
};


#endif // SERVICEHANDLER_H
