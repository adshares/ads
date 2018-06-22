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

    uint32_t path   = m_command->getBlockId() ? m_command->getBlockId() : m_offi.last_path();
    uint32_t users  = m_offi.last_users(m_command->getDestBankId());

    const auto errorCode = m_command->prepareResponse(path, users);
    if (errorCode != ErrorCodes::Code::eNone) {
        DLOG("OnGetAccounts error: %s", ErrorCodes().getErrorMsg(errorCode));
    }

    try {
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        uint32_t sizeOfResponse = m_command->getResponseSize();
        if (!errorCode) {
            response.emplace_back(boost::asio::buffer(&sizeOfResponse, sizeof(uint32_t)));
            response.emplace_back(boost::asio::buffer(m_command->getResponse(), sizeOfResponse));
        }
        boost::asio::write(m_socket, response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

void GetAccountsHandler::onValidate() {
    if (m_command->getDestBankId() > m_offi.last_nodes()) {
        throw ErrorCodes::Code::eBankIncorrect;
    }
}
