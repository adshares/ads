#include "commandservice.h"
#include "command/getaccount.h"
#include "command/setaccountkey.h"
#include "command/sendone.h"
#include "command/sendmany.h"
#include "command/getaccounts.h"
#include "command/broadcastmsg.h"
#include "command/changenodekey.h"
#include "command/getlog.h"
#include "../office.hpp"

CommandService::CommandService(office& office, boost::asio::ip::tcp::socket& socket)
    : m_socket(socket), m_offi(office),
      m_getAccountHandler(office, socket),
      m_setAccountHandler(office, socket),
      m_createNodeHandler(office, socket),
      m_sendOneHandler(office, socket),
      m_sendManyHandler(office, socket),
      m_createAccountHandler(office, socket),
      m_getAccountsHandler(office, socket),
      m_broadcastMsgHandler(office, socket),
      m_getBroadcastMsgHandler(office, socket),
      m_changeNodeKeyHandler(office, socket),
      m_getBlockHandler(office, socket),
      m_getMessageListHandler(office, socket),
      m_getMessageHandler(office, socket),
      m_getLogHandler(office, socket),
      m_getTransactionHandler(office, socket) {
}

void CommandService::onExecute(std::unique_ptr<IBlockCommand> command) {
    user_t usera;
    if(!m_offi.get_user(usera, command->getBankId(), command->getUserId())) {
        ErrorCodes::Code code = ErrorCodes::Code::eWrongSignature;
        DLOG("ERROR: %s\n", ErrorCodes().getErrorMsg(code));
        boost::asio::write(m_socket, boost::asio::buffer(&code, ERROR_CODE_LENGTH));
        return;
    };

    switch(command->getType()) {
    case TXSTYPE_INF:
        m_getAccountHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_KEY:
        m_setAccountHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_BNK:
        m_createNodeHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_PUT:
        m_sendOneHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_MPT:
        m_sendManyHandler.execute(std::move(command), std::move(usera));
    	break;
    case TXSTYPE_USR:
        m_createAccountHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_NOD:
        m_getAccountsHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_BRO:
        m_broadcastMsgHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_BLG:
        m_getBroadcastMsgHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_BKY:
        m_changeNodeKeyHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_NDS:
        m_getBlockHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_MGS:
        m_getMessageListHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_MSG:
        m_getMessageHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_LOG:
        m_getLogHandler.execute(std::move(command), std::move(usera));
        break;
    case TXSTYPE_TXS:
        m_getTransactionHandler.execute(std::move(command), std::move(usera));
        break;
    default:
        DLOG("Command type: %d without handler\n", command->getType());
        break;
    }
}
