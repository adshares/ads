#include "getvipkeyshandler.h"
#include "command/getvipkeys.h"
#include "../office.hpp"
#include "helper/hash.h"
#include "helper/vipkeys.h"

GetVipKeysHandler::GetVipKeysHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetVipKeysHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<GetVipKeys>(dynamic_cast<GetVipKeys*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("GetVipKeys bad_cast caught: %s", bc.what());
        return;
    }
}

void GetVipKeysHandler::onExecute() {
    assert(m_command);
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    char hash[65];
    hash[64]='\0';
    ed25519_key2text(hash, m_command->getVipHash(), 32);
    char filename[128];
    sprintf(filename,"vip/%64s.vip",hash);

    VipKeys vipKeys;
    vipKeys.loadFromFile(filename);
    const uint32_t fileLength = vipKeys.getLength();

    if(fileLength == 0) {
        errorCode = ErrorCodes::Code::eCouldNotReadCorrectVipKeys;
        ELOG("ERROR file %s not found or empty\n", filename);
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if (!errorCode) {
            boost::asio::write(m_socket, boost::asio::buffer(&fileLength, sizeof(fileLength)));
            boost::asio::write(m_socket, boost::asio::buffer(vipKeys.getVipKeys(), fileLength));
        }
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

bool GetVipKeysHandler::onValidate() {
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    auto startedTime = time(NULL);
    int32_t diff = m_command->getTime() - startedTime;

    if(diff>1) {
        DLOG("ERROR: time in the future (%d>1s)\n", diff);
        errorCode = ErrorCodes::Code::eTimeInFuture;
    } else if(m_command->getBankId()!=m_offi.svid) {
        errorCode = ErrorCodes::Code::eBankNotFound;
    }
    else if(m_command->getBankId()!=m_offi.svid) {
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
