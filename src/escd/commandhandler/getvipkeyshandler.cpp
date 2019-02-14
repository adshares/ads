#include "getvipkeyshandler.h"
#include "command/getvipkeys.h"
#include "../office.hpp"
#include "../client.hpp"
#include "helper/hash.h"
#include "helper/vipkeys.h"

GetVipKeysHandler::GetVipKeysHandler(office& office, client& client)
    : CommandHandler(office, client) {
}

void GetVipKeysHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<GetVipKeys>(std::move(command));
}

void GetVipKeysHandler::onExecute() {
    assert(m_command);

    char hash[65];
    hash[64]='\0';
    ed25519_key2text(hash, m_command->getVipHash(), 32);
    char filename[128];
    sprintf(filename,"vip/%64s.vip",hash);

    VipKeys vipKeys;
    vipKeys.loadFromFile(filename);
    const uint32_t fileLength = vipKeys.getLength();

    auto errorCode = ErrorCodes::Code::eNone;

    if(fileLength == 0) {
        errorCode = ErrorCodes::Code::eCouldNotReadCorrectVipKeys;
        ELOG("ERROR file %s not found or empty\n", filename);
    }

    try {
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if (!errorCode) {
            response.emplace_back(boost::asio::buffer(&fileLength, sizeof(fileLength)));
            response.emplace_back(boost::asio::buffer(vipKeys.getVipKeys(), fileLength));
        }
        m_client.sendResponse(response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_command->getUserId(), e.what());
    }
}

void GetVipKeysHandler::onValidate() {
}
