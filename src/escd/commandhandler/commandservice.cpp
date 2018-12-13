#include "commandservice.h"
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
#include "../office.hpp"
#include "../client.hpp"

CommandService::CommandService(office& office, client& client)
    : m_client(client), m_offi(office)
{
}

void CommandService::onExecute(std::unique_ptr<IBlockCommand> command)
{
    user_t      usera;

    if(!m_offi.get_user(usera, command->getBankId(), command->getUserId())) {
        ErrorCodes::Code code = ErrorCodes::Code::eGetUserFail;
        DLOG("ERROR: %s\n", ErrorCodes().getErrorMsg(code));
        m_client.sendError(code);
        return;
    };

    switch(command->getType()) {
    case TXSTYPE_INF: {
        GetAccountHandler getAccountHandler(m_offi, m_client);
        getAccountHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_KEY: {
        SetAccountKeyHandler setAccountHandler(m_offi, m_client);
        setAccountHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_BNK: {
        CreateNodeHandler createNodeHandler(m_offi, m_client);
        createNodeHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_PUT: {
        SendOneHandler sendOneHandler(m_offi, m_client);
        sendOneHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_MPT: {
        SendManyHandler sendManyHandler(m_offi, m_client);
        sendManyHandler.execute(std::move(command), usera);
    	break;
    }
    case TXSTYPE_USR: {
        CreateAccountHandler createAccountHandler(m_offi, m_client);
        createAccountHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_NOD: {
        GetAccountsHandler getAccountsHandler(m_offi, m_client);
        getAccountsHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_BRO: {
        BroadcastMsgHandler broadcastMsgHandler(m_offi, m_client);
        broadcastMsgHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_BLG: {
        GetBroadcastMsgHandler getBroadcastMsgHandler(m_offi, m_client);
        getBroadcastMsgHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_BKY: {
        ChangeNodeKeyHandler changeNodeKeyHandler(m_offi, m_client);
        changeNodeKeyHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_NDS: {
        GetBlockHandler getBlockHandler(m_offi, m_client);
        getBlockHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_MGS: {
        GetMessageListHandler getMessageListHandler(m_offi, m_client);
        getMessageListHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_MSG: {
        GetMessageHandler getMessageHandler(m_offi, m_client);
        getMessageHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_LOG: {
        GetLogHandler getLogHandler(m_offi, m_client);
        getLogHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_TXS: {
        GetTransactionHandler getTransactionHandler(m_offi, m_client);
        getTransactionHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_SUS: {
        SetAccountStatusHandler setAccountStatusHandler(m_offi, m_client);
        setAccountStatusHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_SBS: {
        SetNodeStatusHandler setNodeStatusHandler(m_offi, m_client);
        setNodeStatusHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_UUS: {
        UnsetAccountStatusHandler unsetAccountStatusHandler(m_offi, m_client);
        unsetAccountStatusHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_UBS: {
        UnsetNodeStatusHandler unsetNodeStatusHandler(m_offi, m_client);
        unsetNodeStatusHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_SIG: {
        GetSignaturesHandler getSignaturesHandler(m_offi, m_client);
        getSignaturesHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_GET: {
        RetrieveFundsHandler retrieveFundsHandler(m_offi, m_client);
        retrieveFundsHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_VIP: {
        GetVipKeysHandler getVipKeysHandler(m_offi, m_client);
        getVipKeysHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_BLK: {
        GetBlocksHandler getBlocksHandler(m_offi, m_client);
        getBlocksHandler.execute(std::move(command), usera);
        break;
    }
    case TXSTYPE_SAV: {
        LogAccountHandler logAccountHandler(m_offi, m_client);
        logAccountHandler.execute(std::move(command), usera);
        break;
    }
    default: {
        DLOG("Command type: %d without handler\n", command->getType());
        break;
    }
    }    
}
