#include "sendonehandler.h"
#include "command/sendone.h"
#include "../office.hpp"
#include "helper/hash.h"


SendOneHandler::SendOneHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void SendOneHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<SendOne>(dynamic_cast<SendOne*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("SendOne bad_cast caught: %s", bc.what());
        return;
    }
}

void SendOneHandler::onExecute() {
    assert(m_command);

    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    int64_t     fee{0};
    int64_t     deposit{-1}; // if deposit=0 inform target
    int64_t     deduct{0};
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    deposit = m_command->getDeduct();
    deduct = m_command->getDeduct();
    fee = m_command->getFee();

    //commit changes
    m_usera.msid++;
    m_usera.time=m_command->getTime();
    m_usera.lpath=lpath;

    Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

    uint32_t msid;
    uint32_t mpos;

    if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
        DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        errorCode = ErrorCodes::Code::eMessageSubmitFail;
    } else {
        m_offi.set_user(m_command->getUserId(), m_usera, deduct+fee);

        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getBankId();
        tlog.user   = m_command->getUserId();
        tlog.umid   = m_command->getUserMessageId();
        tlog.nmid   = msid;
        tlog.mpos   = mpos;
        tlog.weight = -deduct - fee;
        memcpy(tlog.info, m_command->getInfoMsg(),32);
        m_offi.put_ulog(m_command->getUserId(),  tlog);

        if (m_command->getBankId() == m_command->getDestBankId()) {
            tlog.type|=0x8000; //incoming
            tlog.node=m_command->getBankId();
            tlog.user=m_command->getUserId();
            tlog.weight=deduct;
            m_offi.put_ulog(m_command->getDestUserId(), tlog);
            if(deposit>=0) {
                m_offi.add_deposit(m_command->getDestUserId(), deposit);
            }
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

bool SendOneHandler::onValidate() {
    int64_t deduct = m_command->getDeduct();
    int64_t fee = m_command->getFee();
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    if(deduct+fee+(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight) {
        DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
             deduct, fee, (uint64_t)(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
        errorCode = ErrorCodes::Code::eLowBalance;
    }
    else if(!m_offi.check_user(m_command->getDestBankId(), m_command->getDestUserId())) {
        // does not check if account closed [consider adding this slow check]
        DLOG("ERROR: bad target user %04X:%08X\n", m_command->getDestBankId(), m_command->getDestUserId());
        errorCode = ErrorCodes::Code::eUserBadTarget;
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
