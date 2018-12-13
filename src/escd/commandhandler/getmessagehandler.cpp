#include "getmessagehandler.h"
#include "command/getmessage.h"
#include "../office.hpp"
#include "../client.hpp"
#include "helper/hash.h"
#include "message.hpp"

GetMessageHandler::GetMessageHandler(office& office, client& client)
    : CommandHandler(office, client) {
}

void GetMessageHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<GetMessage>(std::move(command));
}

void GetMessageHandler::onExecute() {
    assert(m_command);

    auto errorCode = ErrorCodes::Code::eNone;

    uint32_t block = m_command->getBlockTime();
    if(!block) {
        message msg;
        msg.svid = m_command->getDestNode();
        msg.msid = m_command->getMsgId();
        block = msg.load_path();
    }

    message_ptr msg(new message(MSGTYPE_MSG, block, m_command->getDestNode(), m_command->getMsgId()));
    msg->load(BANK_MAX);
    if (!(msg->status & MSGSTAT_SAV)) {
        errorCode = ErrorCodes::Code::eBadLength;
    }

    try {
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            response.emplace_back(boost::asio::buffer(&block, sizeof(uint32_t)));
            response.emplace_back(boost::asio::buffer(msg->data, msg->len));
        }
        m_client.sendResponse(response);
    } catch (std::exception&) {
        DLOG("ERROR responding to client %08X\n",m_command->getUserId());
    }
}

void GetMessageHandler::onValidate() {
}
