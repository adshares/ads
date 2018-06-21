#include "getaccounthandler.h"
#include "command/getaccount.h"
#include "../office.hpp"
#include "command/errorcodes.h"

GetAccountHandler::GetAccountHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
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
    }

    if(m_offi.svid == m_command->getDestNode())
    {
        if(m_command->getDestUser() != m_command->getUserId())
        {
            if(!m_offi.get_user(localAccount, m_command->getDestNode(), m_command->getDestUser()))
            {
                DLOG("FAILED to get user info %08X:%04X\n", m_command->getDestNode(), m_command->getDestUser());
                errorCode = ErrorCodes::Code::eGetUserFail;
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
        boost::asio::write(m_socket, response);
    } catch (std::exception& e) {
        ELOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code GetAccountHandler::onValidate() {
    int32_t diff = m_command->getTime() - time(nullptr);

#ifdef DEBUG
    // this is special, just local info
    if((abs(diff)>22)) {
        DLOG("ERROR: high time difference (%d>2s)\n", diff);
        return ErrorCodes::Code::eHighTimeDifference;
    }
#else
    if((abs(diff)>2)) {
        DLOG("ERROR: high time difference (%d>2s)\n",diff);
        return ErrorCodes::Code::eHighTimeDifference;
    }
#endif

//FIXME, read data also from server
//FIXME, if local account locked, check if unlock was successfull based on time passed after change

    if(m_command->getUserId() != m_offi.svid && m_command->getDestNode() != m_offi.svid) {
        DLOG("ERROR: bad bank for INF abank: %d bbank: %d SVID: %d\n", m_command->getUserId(), m_command->getDestNode(), m_offi.svid );
        return ErrorCodes::Code::eBankNotFound;
    }

    user_t userb;
    if(!m_offi.get_user(userb, m_command->getDestNode(), m_command->getDestUser())) {
        DLOG("FAILED to get user info %08X:%04X\n", m_command->getDestNode(), m_command->getDestUser());
        return ErrorCodes::Code::eGetUserFail;
    }

#ifdef DEBUG
    DLOG("SENDING user info %08X:%04X\n", m_command->getDestNode(), m_command->getDestUser());
#endif

    return ErrorCodes::Code::eNone;
}
