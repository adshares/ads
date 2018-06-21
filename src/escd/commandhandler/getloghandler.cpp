#include "getloghandler.h"
#include "command/getlog.h"
#include "../office.hpp"
#include "helper/hash.h"
#include "helper/hlog.h"

GetLogHandler::GetLogHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetLogHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<GetLog>(std::move(command));
}

void GetLogHandler::onExecute() {
    assert(m_command);
    auto errorCode = ErrorCodes::Code::eNone;

    std::string slog;
    if (!m_offi.get_log(m_command->getBankId(), m_command->getUserId(), m_command->getTime(), slog)) {
        errorCode = ErrorCodes::Code::eGetLogFailed;
    }

    try {
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if (!errorCode) {
            response.emplace_back(boost::asio::buffer(&m_usera, sizeof(user_t)));
            response.emplace_back(boost::asio::buffer(slog.c_str(), slog.size()));
        }
        boost::asio::write(m_socket, response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code GetLogHandler::onValidate() {
    return ErrorCodes::Code::eNone;
}
