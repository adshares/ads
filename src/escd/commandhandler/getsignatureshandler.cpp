#include "getsignatureshandler.h"
#include "command/getsignatures.h"
#include "../office.hpp"
#include "helper/hash.h"

GetSignaturesHandler::GetSignaturesHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetSignaturesHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<GetSignatures>(std::move(command));
}

void GetSignaturesHandler::onExecute() {
    assert(m_command);

    auto errorCode = ErrorCodes::Code::eNone;
    Helper::Signatures signatures;
    signatures.load(m_command->getBlockNumber());

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

ErrorCodes::Code GetSignaturesHandler::onValidate() {
    return ErrorCodes::Code::eNone;
}
