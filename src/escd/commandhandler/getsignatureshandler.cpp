#include "getsignatureshandler.h"
#include "command/getsignatures.h"
#include "../office.hpp"
#include "helper/hash.h"

GetSignaturesHandler::GetSignaturesHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetSignaturesHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<GetSignatures>(dynamic_cast<GetSignatures*>(command.release()));
    } catch (std::bad_cast& bc) {
        ELOG("GetSignatures bad_cast caught: %s\n", bc.what());
        return;
    }
}

void GetSignaturesHandler::onExecute() {
    assert(m_command);

    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    Helper::Signatures signatures(m_command->getBlockNumber());
    signatures.load();

    auto signaturesOk = signatures.getSignaturesOk();
    int32_t signaturesOkSize = signaturesOk.size();
    if (signaturesOkSize == 0) {
        errorCode = ErrorCodes::Code::eGetSignatureUnavailable;
    }

    auto signaturesNo = signatures.getSignaturesNo();
    int32_t signaturesNoSize = signaturesNo.size();

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            boost::asio::write(m_socket, boost::asio::buffer(&signaturesOkSize, sizeof(signaturesOkSize)));
            boost::asio::write(m_socket, boost::asio::buffer(signaturesOk));
            boost::asio::write(m_socket, boost::asio::buffer(&signaturesNoSize, sizeof(signaturesNoSize)));
            boost::asio::write(m_socket, boost::asio::buffer(signaturesNo));
        }
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

bool GetSignaturesHandler::onValidate() {

    auto startedTime = time(NULL);
    int32_t diff = m_command->getTime() - startedTime;
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    if(diff>1) {
        DLOG("ERROR: time in the future (%d>1s)\n", diff);
        errorCode = ErrorCodes::Code::eTimeInFuture;
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
