#include "commandhandler.h"
#include "command/getaccount.h"
#include "command/setaccountkey.h"
#include "command/commandtype.h"
#include "helper/hash.h"
#include "../office.hpp"

CommandHandler::CommandHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : m_offi(office),
      m_socket(socket) {
}

void CommandHandler::execute(std::unique_ptr<IBlockCommand> command, const user_t& usera)
{
    m_usera = usera;

    try {
        executeImpl(std::move(command));
    }
    catch(const ErrorCodes::Code& error) {
        sendErrorToClient(error);
    }
}

void CommandHandler::sendErrorToClient(ErrorCodes::Code error) {
    try {
        boost::asio::write(m_socket, boost::asio::buffer(&error, ERROR_CODE_LENGTH));
    }
    catch(std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

void CommandHandler::executeImpl(std::unique_ptr<IBlockCommand> command) {
    performCommonValidation(*command);

    if(CommandType::eModifying == command->getCommandType()) {
        validateModifyingCommand(*command);
    }

    onInit(std::move(command));
    onValidate();
    onExecute();
}

void CommandHandler::performCommonValidation(IBlockCommand& command) {
    if(!command.checkSignature(m_usera.hash, m_usera.pkey)) {
        throw ErrorCodes::Code::eWrongSignature;
    }

    auto startedTime = time(NULL);
    int32_t diff = command.getTime() - startedTime;

    if(diff>1) {
        DLOG("ERROR: time in the future (%d>1s)\n", diff);
        throw ErrorCodes::Code::eTimeInFuture;
    }

    if(command.getBankId()!=m_offi.svid) {
        throw ErrorCodes::Code::eBankNotFound;
    }
}

void CommandHandler::validateModifyingCommand(IBlockCommand& command) {

    if(!m_offi.svid) {
        throw ErrorCodes::Code::eBankIncorrect;
    }

    if(m_offi.readonly) { //FIXME, notify user.cpp about errors !!!
        throw ErrorCodes::Code::eReadOnlyMode;
    }

    if(m_usera.msid != command.getUserMessageId()) {
        throw ErrorCodes::Code::eBadMsgId;
    }

    if(command.getDeduct() < 0){
        throw ErrorCodes::Code::eAmountBelowZero;
    }

    if(command.getFee() < 0){
        throw ErrorCodes::Code::eFeeBelowZero;
    }

    if(command.getDeduct()+command.getFee()+(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight) {
        DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
             command.getDeduct(), command.getFee(), (uint64_t)(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
        throw ErrorCodes::Code::eLowBalance;
    }
}

CommandHandler::CommitResult CommandHandler::commitChanges(IBlockCommand& command) {
    auto startedTime = time(nullptr);
    uint32_t lpath = startedTime-startedTime%BLOCKSEC;

    m_usera.msid++;
    m_usera.time = command.getTime();
    m_usera.lpath = lpath;

    Helper::create256signhash(command.getSignature(), command.getSignatureSize(), m_usera.hash, m_usera.hash);

    uint32_t msid, mpos;

    if(!m_offi.add_msg(command, msid, mpos)) {
        DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        return {ErrorCodes::Code::eMessageSubmitFail, msid, mpos};
    }

    m_offi.set_user(command.getUserId(), m_usera, command.getDeduct() + command.getFee());
    return {ErrorCodes::Code::eNone, msid, mpos};
}
