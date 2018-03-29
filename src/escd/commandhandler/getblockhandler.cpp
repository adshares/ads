#include "getblockhandler.h"
#include "command/getblock.h"
#include "../office.hpp"
#include "helper/hash.h"
#include "helper/hlog.h"

GetBlockHandler::GetBlockHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetBlockHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<GetBlock>(dynamic_cast<GetBlock*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("GetBlock bad_cast caught: %s", bc.what());
        return;
    }
}

void GetBlockHandler::onExecute() {
    assert(m_command);
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    errorCode = m_command->prepareResponse();

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if (!errorCode) {
            // only header
            boost::asio::write(m_socket, boost::asio::buffer(m_command->getResponse(), m_command->getResponseSize()));
            // nodes
            boost::asio::write(m_socket, boost::asio::buffer(m_command->m_responseNodes));
            // hlog
            Helper::Hlog hlog(m_command->getBlockId());
            hlog.load();
            boost::asio::write(m_socket, boost::asio::buffer(hlog.data, 4 + hlog.total));

        }
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

bool GetBlockHandler::onValidate() {
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    if(m_command->getBankId()!=m_offi.svid) {
        errorCode = ErrorCodes::Code::eBankNotFound;
    }

    if (errorCode) {
        try {
            boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        } catch (std::exception& e) {
            DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
        }
        return false;
    }

    return true;
}
