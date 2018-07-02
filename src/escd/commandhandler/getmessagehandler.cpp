#include "getmessagehandler.h"
#include "command/getmessage.h"
#include "../office.hpp"
#include "helper/hash.h"
#include "message.hpp"

GetMessageHandler::GetMessageHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetMessageHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<GetMessage>(std::move(command));
}

void GetMessageHandler::onExecute() {
    assert(m_command);

    auto errorCode = ErrorCodes::Code::eNone;

    message_ptr msg(new message(MSGTYPE_MSG, m_command->getBlockTime(), m_command->getDestNode(), m_command->getMsgId()));
    msg->load(BANK_MAX);
    if (!(msg->status & MSGSTAT_SAV)) {
        errorCode = ErrorCodes::Code::eBadLength;
    }

    try {
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            response.emplace_back(boost::asio::buffer(msg->data, msg->len));
        }
        boost::asio::write(m_socket, response);
    } catch (std::exception&) {
        DLOG("ERROR responding to client %08X\n",m_usera.user);
    }
}

void GetMessageHandler::onValidate() {
}
