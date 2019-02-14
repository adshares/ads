#include "getblockhandler.h"
#include "command/getblock.h"
#include "../office.hpp"
#include "../client.hpp"
#include "helper/hash.h"
#include "helper/hlog.h"

GetBlockHandler::GetBlockHandler(office& office, client& client)
    : CommandHandler(office, client) {
}

void GetBlockHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<GetBlock>(std::move(command));
}

void GetBlockHandler::onExecute() {
    assert(m_command);

    const auto errorCode = m_command->prepareResponse();

    try {
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        Helper::Hlog hlog(m_command->getBlockId());
        hlog.load();

        if (!errorCode) {
            // only header
            response.emplace_back(boost::asio::buffer(m_command->getResponse(), m_command->getResponseSize()));
            // nodes
            response.emplace_back(boost::asio::buffer(m_command->m_responseNodes));
            // hlog
            response.emplace_back(boost::asio::buffer(hlog.data, 4 + hlog.total));
        }
        m_client.sendResponse(response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_command->getUserId(), e.what());
    }
}

void GetBlockHandler::onValidate() {
}
