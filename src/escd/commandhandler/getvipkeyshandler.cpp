#include "getvipkeyshandler.h"
#include "command/getvipkeys.h"
#include "../office.hpp"
#include "helper/hash.h"
#include <fstream>

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

    std::stringstream buffer;
    std::ifstream file(filename, std::ifstream::in | std::ifstream::binary);
    if(file.is_open()) {
        buffer << file.rdbuf();
        file.close();
    }
    else {
        ELOG("ERROR opening %s\n",filename);
        errorCode = ErrorCodes::Code::eVipKeysFileCouldNotBeOpened;
    }

    const auto fileContent = buffer.str();
    const uint32_t fileLength = fileContent.length();
    if(!errorCode && fileLength == 0) {
        ELOG("ERROR empty %s\n", filename);
        errorCode = ErrorCodes::Code::eVipKeysFileEmpty;
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if (!errorCode) {
            boost::asio::write(m_socket, boost::asio::buffer(&fileLength, sizeof(fileLength)));
            boost::asio::write(m_socket, boost::asio::buffer(fileContent.c_str(), fileLength));
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
