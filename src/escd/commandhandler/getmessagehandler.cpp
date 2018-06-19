#include "getmessagehandler.h"
#include "command/getmessage.h"
#include "../office.hpp"
#include "helper/hash.h"
#include "message.hpp"

GetMessageHandler::GetMessageHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}


void GetMessageHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<GetMessage>(dynamic_cast<GetMessage*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("GetMessage bad_cast caught: %s", bc.what());
        return;
    }

}

void GetMessageHandler::onExecute() {
    assert(m_command);

    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    message_ptr msg(new message(MSGTYPE_MSG, m_command->getBlockTime(), m_command->getDestNode(), m_command->getMsgId()));
    msg->load(BANK_MAX);
    if (!(msg->status & MSGSTAT_SAV)) {
        errorCode = ErrorCodes::Code::eBadLength;
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            boost::asio::write(m_socket, boost::asio::buffer(msg->data, msg->len));
        }
    } catch (std::exception&) {
        DLOG("ERROR responding to client %08X\n",m_usera.user);
    }
}

ErrorCodes::Code GetMessageHandler::onValidate() {
    return ErrorCodes::Code::eNone;
}
