#include "getaccounthandler.h"
#include "command/getaccount.h"
#include "../office.hpp"
#include "../client.hpp"
#include "command/errorcodes.h"

GetAccountHandler::GetAccountHandler(office& office, client& client)
    : CommandHandler(office, client) {
}

void GetAccountHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<GetAccount>(std::move(command));
}

void GetAccountHandler::onExecute() {
    assert(m_command);
    auto errorCode = ErrorCodes::Code::eNone;

    user_t localAccount(m_usera);
    user_t remoteAccount;

    if(!m_offi.get_user_global(remoteAccount, m_command->getDestNode(), m_command->getDestUser())) {
        DLOG("FAILED to get global user info %08X:%04X\n", m_command->getDestNode(), m_command->getDestUser());
        errorCode = ErrorCodes::Code::eGetGlobalUserFail;
        return m_client.sendError(errorCode);
    }

    if(m_offi.svid == m_command->getDestNode())
    {
        if(m_command->getDestUser() != m_command->getUserId())
        {
            if(!m_offi.get_user(localAccount, m_command->getDestNode(), m_command->getDestUser()))
            {
                DLOG("FAILED to get user info %08X:%04X\n", m_command->getDestNode(), m_command->getDestUser());
                errorCode = ErrorCodes::Code::eGetUserFail;
                return m_client.sendError(errorCode);
            }
        }
    }
    else {
        localAccount = remoteAccount;
    }

    try {
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));

        if(!errorCode) {
            response.emplace_back(boost::asio::buffer(&localAccount, sizeof(user_t)));
            response.emplace_back(boost::asio::buffer(&remoteAccount, sizeof(user_t)));
        }
        m_client.sendResponse(response);
    } catch (std::exception& e) {
        ELOG("Responding to client %08X error: %s\n", m_command->getUserId(), e.what());
    }
}

void GetAccountHandler::onValidate() {
    const int32_t diff = m_command->getTime() - time(nullptr);

    // this is special, just local info
    if((abs(diff)>10)) {
        DLOG("ERROR: high time difference (%d>2s)\n", diff);
        throw ErrorCodes::Code::eHighTimeDifference;
    }

//FIXME, read data also from server
//FIXME, if local account locked, check if unlock was successfull based on time passed after change

    if(m_command->getBankId() != m_offi.svid && m_command->getDestNode() != m_offi.svid) {
        DLOG("ERROR: bad bank for INF abank: %d bbank: %d SVID: %d\n", m_command->getUserId(), m_command->getDestNode(), m_offi.svid );
        throw ErrorCodes::Code::eBankNotFound;
    }

    user_t userb;
    if(!m_offi.get_user(userb, m_command->getDestNode(), m_command->getDestUser())) {
        DLOG("FAILED to get user info %08X:%04X\n", m_command->getDestNode(), m_command->getDestUser());
        throw ErrorCodes::Code::eGetUserFail;
    }

#ifdef DEBUG
    DLOG("SENDING user info %08X:%04X\n", m_command->getDestNode(), m_command->getDestUser());
#endif
}
