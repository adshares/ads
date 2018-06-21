#include "getaccountshandler.h"
#include "command/getaccounts.h"
#include "../office.hpp"
#include "command/errorcodes.h"

GetAccountsHandler::GetAccountsHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetAccountsHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<GetAccounts>(std::move(command));
}

void GetAccountsHandler::onExecute() {
    assert(m_command);

    uint32_t path = m_offi.last_path();
    uint32_t users = m_offi.last_users(m_command->getDestBankId());

    const auto errorCode = m_command->prepareResponse(path, users);
    if (errorCode != ErrorCodes::Code::eNone) {
        DLOG("OnGetAccounts error: %s", ErrorCodes().getErrorMsg(errorCode));
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if (!errorCode) {
            uint32_t sizeOfResponse = m_command->getResponseSize();
            boost::asio::write(m_socket, boost::asio::buffer(&sizeOfResponse, sizeof(uint32_t)));
            boost::asio::write(m_socket, boost::asio::buffer(m_command->getResponse(), sizeOfResponse));
        }
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code GetAccountsHandler::onValidate() {
    uint32_t path=m_offi.last_path();

    if (m_command->getBlockId() != path) {
        return ErrorCodes::Code::eBadPath;
    }

    if (m_command->getDestBankId() > m_offi.last_nodes()) {
        return ErrorCodes::Code::eBankIncorrect;
    }

    return ErrorCodes::Code::eNone;
}
