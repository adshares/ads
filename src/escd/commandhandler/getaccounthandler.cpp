#include "getaccounthandler.h"
#include "command/getaccount.h"
#include "../office.hpp"
#include "command/errorcodes.h"

GetAccountHandler::GetAccountHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetAccountHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<GetAccount>(dynamic_cast<GetAccount*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("OnGetAccount bad_cast caught: %s", bc.what());
        return;
    }
}

void GetAccountHandler::onExecute() {
    assert(m_command);
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    userinfo&   data    = m_command->getDataStruct().info;
    std::vector<boost::asio::const_buffer> response;

    user_t localAccount(m_usera);
    user_t remoteAccount;

    if(!m_offi.get_user_global(remoteAccount, data.bbank, data.buser)) {
        DLOG("FAILED to get global user info %08X:%04X\n", data.bbank, data.buser);
        errorCode = ErrorCodes::Code::eGetGlobalUserFail;
    }

    if(m_offi.svid == data.bbank)
    {
        if(data.buser != data.auser)
        {
            if(!m_offi.get_user(localAccount, data.bbank, data.buser))
            {
                DLOG("FAILED to get user info %08X:%04X\n", data.bbank, data.buser);
                errorCode = ErrorCodes::Code::eGetUserFail;
            }
        }
    }
    else {
        localAccount = remoteAccount;
    }

    response.emplace_back(boost::asio::buffer(&localAccount, sizeof(user_t)));
    response.emplace_back(boost::asio::buffer(&remoteAccount, sizeof(user_t)));

    if (errorCode) {
        response.clear();
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        boost::asio::write(m_socket, response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code GetAccountHandler::onValidate() {
    userinfo& data = m_command->getDataStruct().info;
    int32_t diff = data.ttime - time(nullptr);

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

    if(data.abank != m_offi.svid && data.bbank != m_offi.svid) {
        DLOG("ERROR: bad bank for INF abank: %d bbank: %d SVID: %d\n", data.abank, data.bbank, m_offi.svid );
        return ErrorCodes::Code::eBankNotFound;
    }

    user_t userb;
    if(!m_offi.get_user(userb, data.bbank, data.buser)) {
        DLOG("FAILED to get user info %08X:%04X\n", data.bbank, data.buser);
        return ErrorCodes::Code::eGetUserFail;
    }

#ifdef DEBUG
    DLOG("SENDING user info %08X:%04X\n", data.bbank, data.buser);
#endif

    return ErrorCodes::Code::eNone;
}
