#include "factory.h"
#include "pods.h"
#include "getaccount.h"
#include "setaccountkey.h"
#include "createnode.h"
#include "sendone.h"
#include "sendmany.h"
#include "createaccount.h"
#include "getaccounts.h"
#include "broadcastmsg.h"
#include "getbroadcastmsg.h"
#include "changenodekey.h"
#include "getblock.h"
#include "getmessage.h"
#include "getmessagelist.h"
#include "getlog.h"
#include "gettransaction.h"
#include "connected.h"
#include "setaccountstatus.h"
#include "setnodestatus.h"
#include "unsetaccountstatus.h"

namespace command {

std::unique_ptr<IBlockCommand> factory::makeCommand(uint8_t type) {
    std::unique_ptr<IBlockCommand>  command;

    switch(type) {
    case TXSTYPE_CON:
        command = std::make_unique<Connected>();
        break;
    case TXSTYPE_INF:
        command = std::make_unique<GetAccount>();
        break;
    case TXSTYPE_KEY:
        command = std::make_unique<SetAccountKey>();
        break;
    case TXSTYPE_BNK:
        command = std::make_unique<CreateNode>();
        break;
    case TXSTYPE_PUT:
        command = std::make_unique<SendOne>();
        break;
    case TXSTYPE_MPT:
        command = std::make_unique<SendMany>();
        break;
    case TXSTYPE_USR:
        command = std::make_unique<CreateAccount>();
        break;
    case TXSTYPE_NOD:
        command = std::make_unique<GetAccounts>();
        break;
    case TXSTYPE_BRO:
        command = std::make_unique<BroadcastMsg>();
        break;
    case TXSTYPE_BLG:
        command = std::make_unique<GetBroadcastMsg>();
        break;
    case TXSTYPE_BKY:
        command = std::make_unique<ChangeNodeKey>();
        break;
    case TXSTYPE_NDS:
        command = std::make_unique<GetBlock>();
        break;
    case TXSTYPE_MGS:
        command = std::make_unique<GetMessageList>();
        break;
    case TXSTYPE_MSG:
        command = std::make_unique<GetMessage>();
        break;
    case TXSTYPE_LOG:
        command = std::make_unique<GetLog>();
        break;
    case TXSTYPE_TXS:
        command = std::make_unique<GetTransaction>();
        break;
    case TXSTYPE_SUS:
        command = std::make_unique<SetAccountStatus>();
        break;
    case TXSTYPE_SBS:
        command = std::make_unique<SetNodeStatus>();
        break;
    case TXSTYPE_UUS:
        command = std::make_unique<UnsetAccountStatus>();
        break;
    default:
        break;
    }

    //assert(command);
    //assert(command->getType() != type);

    return command;
}

}
