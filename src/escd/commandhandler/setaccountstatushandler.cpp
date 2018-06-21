#include "setaccountstatushandler.h"
#include "command/setaccountstatus.h"
#include "../office.hpp"
#include "helper/hash.h"

SetAccountStatusHandler::SetAccountStatusHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void SetAccountStatusHandler::onInit(std::unique_ptr<IBlockCommand> command) {
     m_command = init<SetAccountStatus>(std::move(command));
}

void SetAccountStatusHandler::onExecute() {
    assert(m_command);

    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    int64_t     fee             = m_command->getFee();
    int64_t     deduct          = m_command->getDeduct();

    //commit changes
    m_usera.msid++;
    m_usera.time  = m_command->getTime();
    m_usera.lpath = lpath;

    Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

    auto errorCode = ErrorCodes::Code::eNone;
    uint32_t msid, mpos;

    if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
        ELOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        errorCode = ErrorCodes::Code::eMessageSubmitFail;
    }

    if(!errorCode) {
        if(!m_offi.set_account_status(m_command->getDestUserId(), m_command->getStatus())) {
            ELOG("ERROR: status submission failed");
            errorCode = ErrorCodes::Code::eStatusSubmitFail;
        }
    }

    if(!errorCode) {
        m_offi.set_user(m_command->getUserId(), m_usera, deduct+fee);

        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getDestBankId();
        tlog.user   = m_command->getDestUserId();
        tlog.umid   = m_command->getUserMessageId();
        tlog.nmid   = msid;
        tlog.mpos   = mpos;
        tlog.weight = 0;

        tInfo info;
        info.weight = m_command->getStatus();
        info.deduct = m_command->getDeduct();
        info.fee = m_command->getFee();
        info.stat = m_usera.stat;
        memcpy(info.pkey, m_usera.pkey, sizeof(info.pkey));
        memcpy(tlog.info, &info, sizeof(tInfo));


        m_offi.put_ulog(m_command->getUserId(), tlog);

        if(m_command->getBankId() == m_command->getDestBankId()) {
            tlog.type|=0x8000; //incoming
            tlog.node=m_command->getBankId();
            tlog.user=m_command->getUserId();

            m_offi.put_ulog(m_command->getDestUserId(),tlog);
        }
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            commandresponse response{m_usera, msid, mpos};
            boost::asio::write(m_socket, boost::asio::buffer(&response, sizeof(response)));
        }
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code SetAccountStatusHandler::onValidate() {
    if(!m_offi.check_user(m_command->getDestBankId(),m_command->getDestUserId())) {
        DLOG("ERROR: bad target user %04X:%08X\n", m_command->getDestBankId(), m_command->getDestUserId());
        return ErrorCodes::Code::eUserBadTarget;
    }

    if(m_command->getDestBankId() != m_offi.svid) {
        DLOG("ERROR: changing account status on remote node is not allowed");
        return ErrorCodes::Code::eAccountStatusOnRemoteNode;
    }

    if(0x0 != (m_command->getStatus()&0x1)) {
        DLOG("ERROR: not authorized to change first bit for user %08X \n", m_command->getDestUserId());
        return ErrorCodes::Code::eAuthorizationError;
    }

    if(m_command->getUserId() && m_command->getUserId() != m_command->getDestUserId() &&
            (0x0 != (m_command->getStatus()&0xFFF0))) {

        DLOG("ERROR: not authorized to change higher bits (%04X) for user %08X \n",
            m_command->getStatus(), m_command->getDestUserId());

        return ErrorCodes::Code::eAuthorizationError;
    }

    return ErrorCodes::Code::eNone;
}

