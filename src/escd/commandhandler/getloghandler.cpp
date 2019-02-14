#include "getloghandler.h"
#include "command/getlog.h"
#include "../office.hpp"
#include "../client.hpp"
#include "helper/hash.h"

GetLogHandler::GetLogHandler(office& office, client& client)
    : CommandHandler(office, client) {
}

void GetLogHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<GetLog>(std::move(command));
}

void GetLogHandler::onExecute() {
    assert(m_command);
    auto errorCode = ErrorCodes::Code::eNone;

    user_t      account;

    uint16_t node = m_command->getDestBankId();
    uint32_t user = m_command->getDestUserId();

    if(!node && !user) {
        node = m_command->getBankId();
        user = m_command->getUserId();
        account = m_usera;
    }

    std::string slog;
    if (!m_offi.get_log(node, user, m_command->getTime(), m_command->getFull(), slog)) {
        errorCode = ErrorCodes::Code::eGetLogFailed;
    }

    if(node != m_command->getBankId() || user != m_command->getUserId())
    {
        if(!m_offi.get_user(account, node, user)) {
            ErrorCodes::Code code = ErrorCodes::Code::eGetUserFail;
            DLOG("ERROR: %s\n", ErrorCodes().getErrorMsg(code));
            m_client.sendError(code);
            return;
        };
    }

    try {
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if (!errorCode) {
            response.emplace_back(boost::asio::buffer(&account, sizeof(user_t)));
            response.emplace_back(boost::asio::buffer(slog.c_str(), slog.size()));
        }
        m_client.sendResponse(response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_command->getUserId(), e.what());
    }
}

void GetLogHandler::onValidate() {
}
