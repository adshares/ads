#include "commandhandler.h"
#include "command/getaccount.h"
#include "command/setaccountkey.h"
#include "command/commandtype.h"
#include "helper/hash.h"
#include "../office.hpp"
#include "../client.hpp"
#include "../client.hpp"

CommandHandler::CommandHandler(office& office, client& client)
    : m_offi(office),
      m_client(client),
      m_socket(client.socket()) {
}

void CommandHandler::execute(std::unique_ptr<IBlockCommand> command, const user_t& usera)
{
    m_usera = usera;

    try {
        executeImpl(std::move(command));
    }
    catch(CommandException& error) {
        m_client.sendError((ErrorCodes::Code)error.getErrorCode(), error.getErrorInfo());
    }
    catch(const ErrorCodes::Code& error) {
        m_client.sendError(error);
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

    if(diff>3) {
        DLOG("ERROR: time in the future (%d>3s)\n", diff);
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
    std::string info = "";
    int64_t available_balance = m_usera.weight - (command.getUserId() ? USER_MIN_MASS:BANK_MIN_UMASS);

    if(m_usera.weight - command.getDeduct() - command.getFee() < (command.getUserId() ? USER_MIN_MASS:BANK_MIN_UMASS)) {
        info = "Account balance after transaction must be at least " + Helper::print_amount((command.getUserId() ? USER_MIN_MASS:BANK_MIN_UMASS));
    }

    int64_t next_now = m_offi.last_path() + 2 * BLOCKSEC;

    if(((next_now/BLOCKSEC)%BLOCKDIV == 0) && (m_usera.lpath < next_now - ACCOUNT_DORMANT_AGE)) {
        available_balance += -TXS_GOK_FEE(m_usera.weight);
        info = "Available balance reduced to " + Helper::print_amount(available_balance) + " for upcoming dormant fee. Issue cheaper tx first.";
    }

    if(command.getDeduct()+command.getFee() > available_balance) {
        DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
             command.getDeduct(), command.getFee(), (uint64_t)(command.getUserId() ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
        throw CommandException(ErrorCodes::Code::eLowBalance, info);
    }
}

CommandHandler::CommitResult CommandHandler::commitChanges(IBlockCommand& command) {
    auto startedTime = time(nullptr);
    uint32_t lpath = startedTime-startedTime%BLOCKSEC;

    m_usera.msid++;
    m_usera.time = command.getTime();
    m_usera.lpath = lpath;
    m_usera.node = 0;
    m_usera.user = 0;

    Helper::create256signhash(command.getSignature(), command.getSignatureSize(), m_usera.hash, m_usera.hash);

    uint32_t msid, mpos;

    if(!m_offi.add_msg(command, msid, mpos)) {
        DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        return {ErrorCodes::Code::eMessageSubmitFail, msid, mpos};
    }

    m_offi.set_user(command.getUserId(), m_usera, command.getDeduct() + command.getFee());
    return {ErrorCodes::Code::eNone, msid, mpos};
}
