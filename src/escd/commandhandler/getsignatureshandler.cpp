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

    const auto signaturesOk = signatures.getSignaturesOk();
    const int32_t signaturesOkSize = signaturesOk.size();
    if (signaturesOkSize == 0) {
        errorCode = ErrorCodes::Code::eGetSignatureUnavailable;
    }

    const auto signaturesNo = signatures.getSignaturesNo();
    const int32_t signaturesNoSize = signaturesNo.size();

    try {
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            response.emplace_back(boost::asio::buffer(&signaturesOkSize, sizeof(signaturesOkSize)));
            response.emplace_back(boost::asio::buffer(signaturesOk));
            response.emplace_back(boost::asio::buffer(&signaturesNoSize, sizeof(signaturesNoSize)));
            response.emplace_back(boost::asio::buffer(signaturesNo));
        }
        boost::asio::write(m_socket, response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

void GetSignaturesHandler::onValidate() {
}
