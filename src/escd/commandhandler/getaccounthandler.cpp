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

    user_t userb;
    if(data.abank != data.bbank || data.auser != data.buser) {
        if(!m_offi.get_user(userb, data.bbank, data.buser)) {
            DLOG("FAILED to get user info %08X:%04X\n", data.bbank, data.buser);
            errorCode = ErrorCodes::Code::eGetUserFail;
        } else {
            response.push_back(boost::asio::buffer(&userb, sizeof(user_t)));
        }
    } else {
        response.push_back(boost::asio::buffer(&m_usera, sizeof(user_t)));
    }

    //TODO Check why two times we are sending data.
    if(data.bbank) {
        if(!m_offi.get_user_global(userb, data.bbank, data.buser)) {
            DLOG("FAILED to get global user info %08X:%04X\n", data.bbank, data.buser);
            errorCode = ErrorCodes::Code::eGetGlobalUserFail;
        } else {
            response.push_back(boost::asio::buffer(&userb, sizeof(user_t)));
        }
    }

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

bool GetAccountHandler::onValidate() {
    userinfo&   data    = m_command->getDataStruct().info;
    int32_t     diff    = data.ttime - time(nullptr);
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    user_t userb;

#ifdef DEBUG
    // this is special, just local info
    if((abs(diff)>22)) {
        DLOG("ERROR: high time difference (%d>2s)\n", diff);
        errorCode = ErrorCodes::Code::eHighTimeDifference;
    }
#else
    if((abs(diff)>2)) {
        DLOG("ERROR: high time difference (%d>2s)\n",diff);
        return false;
    }
#endif

//FIXME, read data also from server
//FIXME, if local account locked, check if unlock was successfull based on time passed after change
    if (!errorCode) {
        if(data.abank != m_offi.svid && data.bbank != m_offi.svid) {
            DLOG("ERROR: bad bank for INF abank: %d bbank: %d SVID: %d\n", data.abank, data.bbank, m_offi.svid );
            errorCode = ErrorCodes::Code::eBankNotFound;
        } else if(!m_offi.get_user(userb, data.bbank, data.buser)) {
            DLOG("FAILED to get user info %08X:%04X\n", data.bbank, data.buser);
            errorCode = ErrorCodes::Code::eGetUserFail;
        }
    }

    if (errorCode) {
        try {
            boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        } catch (std::exception& e) {
            DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
        }
        return false;
    }

#ifdef DEBUG
    DLOG("SENDING user info %08X:%04X\n", data.bbank, data.buser);
#endif

    return true;
}
