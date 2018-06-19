#include "commandhandler.h"
#include "command/getaccount.h"
#include "command/setaccountkey.h"
#include "command/commandtype.h"
#include "../office.hpp"


CommandHandler::CommandHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : m_offi(office),
      m_socket(socket) {
}

void CommandHandler::execute(std::unique_ptr<IBlockCommand> command, const user_t& usera)
{
    m_usera = usera;

    auto error = executeImpl(std::move(command));

    if(error) {
        try {
            boost::asio::write(m_socket, boost::asio::buffer(&error, ERROR_CODE_LENGTH));
        }
        catch(std::exception& e) {
            DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
        }
    }
}

ErrorCodes::Code CommandHandler::executeImpl(std::unique_ptr<IBlockCommand> command) {
    auto error = ErrorCodes::Code::eNone;

    if((error = initialValidation(*command))) {
        return error;
    }

    onInit(std::move(command));

    if((error = onValidate())) {
        return error;
    }

    onExecute();
    return error;
}

ErrorCodes::Code CommandHandler::initialValidation(IBlockCommand& command) {
    auto error = ErrorCodes::Code::eNone;

    if((error = performCommonValidation(command))) {
        return error;
    }

    if(CommandType::eModifying == command.getCommandType()) {
        error = validateModifyingCommand(command);
    }

    return error;
}

ErrorCodes::Code CommandHandler::performCommonValidation(IBlockCommand& command) {
    if(!command.checkSignature(m_usera.hash, m_usera.pkey)) {
        return ErrorCodes::Code::eWrongSignature;
    }

    auto startedTime = time(NULL);
    int32_t diff = command.getTime() - startedTime;

    if(diff>1) {
        DLOG("ERROR: time in the future (%d>1s)\n", diff);
        return ErrorCodes::Code::eTimeInFuture;
    }

    if(command.getBankId()!=m_offi.svid) {
        return ErrorCodes::Code::eBankNotFound;
    }

    return ErrorCodes::Code::eNone;
}

ErrorCodes::Code CommandHandler::validateModifyingCommand(IBlockCommand& command) {

    if(!m_offi.svid) {
        return ErrorCodes::Code::eBankIncorrect;
    }

    if(m_offi.readonly) { //FIXME, notify user.cpp about errors !!!
        return ErrorCodes::Code::eReadOnlyMode;
    }

    if(m_usera.msid != command.getUserMessageId()) {
        return ErrorCodes::Code::eBadMsgId;
    }

    if(command.getDeduct() < 0){
        return ErrorCodes::Code::eAmountBelowZero;
    }

    if(command.getFee() < 0){
        return ErrorCodes::Code::eFeeBelowZero;
    }

    if(command.getDeduct()+command.getFee()+(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight) {
        DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
             command.getDeduct(), command.getFee(), (uint64_t)(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
        return ErrorCodes::Code::eLowBalance;
    }
    return ErrorCodes::Code::eNone;
}
