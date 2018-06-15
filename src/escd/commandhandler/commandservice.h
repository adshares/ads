#ifndef SERVICEHANDLER_H
#define SERVICEHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "getaccounthandler.h"
#include "setaccountkeyhandler.h"
#include "createnodehandler.h"
#include "sendonehandler.h"
#include "sendmanyhandler.h"
#include "createaccounthandler.h"
#include "getaccountshandler.h"
#include "broadcastmsghandler.h"
#include "getbroadcastmsghandler.h"
#include "changenodekeyhandler.h"
#include "getblockhandler.h"
#include "getmessagelisthandler.h"
#include "getmessagehandler.h"
#include "getloghandler.h"
#include "gettransactionhandler.h"
#include "setaccountstatushandler.h"
#include "setnodestatushandler.h"
#include "unsetaccountstatushandler.h"
#include "unsetnodestatushandler.h"
#include "getsignatureshandler.h"
#include "retrievefundshandler.h"
#include "getvipkeyshandler.h"
#include "getblockshandler.h"
#include "logaccounthandler.h"

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
    boost::asio::ip::tcp::socket& m_socket;     ///< reference to scoket, required for sending errors

    office&                   m_offi;              ///< reference to office object.
    GetAccountHandler         m_getAccountHandler; ///< get account and get me command object handler.
    SetAccountKeyHandler      m_setAccountHandler; ///< set account key command object handler.
    CreateNodeHandler	      m_createNodeHandler; ///< create node command object handler.
    SendOneHandler            m_sendOneHandler;    ///< send one command handler
    SendManyHandler           m_sendManyHandler;   ///< send many command handler
    CreateAccountHandler      m_createAccountHandler;  ///< create account handler
    GetAccountsHandler        m_getAccountsHandler;   ///< get accounts command handler
    BroadcastMsgHandler       m_broadcastMsgHandler;   ///< broadcast handler
    GetBroadcastMsgHandler    m_getBroadcastMsgHandler;   ///< get broadcast message handler
    ChangeNodeKeyHandler      m_changeNodeKeyHandler;  ///< change node key command handler
    GetBlockHandler           m_getBlockHandler;       ///< get block command handler
    GetMessageListHandler     m_getMessageListHandler; ///< get message list handler
    GetMessageHandler         m_getMessageHandler;     ///< get message handler
    GetLogHandler             m_getLogHandler;         ///< get log handler
    GetTransactionHandler     m_getTransactionHandler; ///< get transaction handler
    SetAccountStatusHandler   m_setAccountStatusHandler; ///< set account status handler
    SetNodeStatusHandler      m_setNodeStatusHandler; ///< set node status handler
    UnsetAccountStatusHandler m_unsetAccountStatusHandler; ///< unset account status handler
    UnsetNodeStatusHandler    m_unsetNodeStatusHandler; ///< unset node status handler
    GetSignaturesHandler      m_getSignaturesHandler; ///< get signatures handler
    RetrieveFundsHandler      m_retrieveFundsHandler; ///< retrieve funds handler
    GetVipKeysHandler         m_getVipKeysHandler; ///< get vip keys handler
    GetBlocksHandler          m_getBlocksHandler; ///< get blocks handler
    LogAccountHandler         m_logAccountHandler; ///< log account handler
};


#endif // SERVICEHANDLER_H
