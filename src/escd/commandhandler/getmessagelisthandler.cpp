#include "getmessagelisthandler.h"
#include "command/getmessagelist.h"
#include "../office.hpp"
#include "helper/hash.h"

GetMessageListHandler::GetMessageListHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}


void GetMessageListHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<GetMessageList>(dynamic_cast<GetMessageList*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("GetMessageList bad_cast caught: %s", bc.what());
        return;
    }

}

void GetMessageListHandler::onExecute() {
    assert(m_command);

    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    errorCode = m_command->prepareResponse();

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            uint32_t no_of_msg = m_command->m_responseMessageList.size();
            boost::asio::write(m_socket, boost::asio::buffer(&no_of_msg, sizeof(no_of_msg)));
            boost::asio::write(m_socket, boost::asio::buffer(m_command->m_responseTxnHash, sizeof(m_command->m_responseTxnHash)));
            boost::asio::write(m_socket, boost::asio::buffer(m_command->m_responseMessageList));
        }
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}



bool GetMessageListHandler::onValidate() {
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    if(m_command->getBankId()!=m_offi.svid) {
        errorCode = ErrorCodes::Code::eBankNotFound;
        ELOG("GetMessageList error: %s\n", ErrorCodes().getErrorMsg(errorCode));
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
