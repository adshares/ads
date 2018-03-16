#include "getaccountshandler.h"
#include "command/getaccounts.h"
#include "../office.hpp"
#include "command/errorcodes.h"

GetAccountsHandler::GetAccountsHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetAccountsHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<GetAccounts>(dynamic_cast<GetAccounts*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("OnGetAccounts bad_cast caught: %s", bc.what());
        return;
    }
}

void GetAccountsHandler::onExecute() {
    assert(m_command);

    uint32_t path = m_offi.last_path();
    uint32_t users = m_offi.last_users(m_command->getDestBankId());

    ErrorCodes::Code errorCode = m_command->prepareResponse(path, users);
    if (errorCode != ErrorCodes::Code::eNone) {
        DLOG("OnGetAccounts error: %s", ErrorCodes().getErrorMsg(errorCode));
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(m_command->getResponse(), m_command->getResponseSize()));
    } catch (std::exception&) {
        DLOG("ERROR responding to client %08X\n",m_usera.user);
    }
}

bool GetAccountsHandler::onValidate() {
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    uint32_t path=m_offi.last_path();

    if (m_command->getBlockId() != path) {
        errorCode = ErrorCodes::Code::eBadPath;
    }
    else if (m_command->getDestBankId() > m_offi.last_nodes()) {
        errorCode = ErrorCodes::Code::eBadNode;
    }

    if (errorCode) {
        boost::asio::write(m_socket,boost::asio::buffer((uint8_t*)&errorCode,4));
        return false;
    }

    return true;
}
