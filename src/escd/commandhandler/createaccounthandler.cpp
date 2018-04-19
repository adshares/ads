#include "createaccounthandler.h"
#include "command/createaccount.h"
#include "../office.hpp"
#include "helper/hash.h"
#include "command/errorcodes.h"

CreateAccountHandler::CreateAccountHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void CreateAccountHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<CreateAccount>(dynamic_cast<CreateAccount*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("CreateAccount bad_cast caught: %s", bc.what());
        return;
    }
}

void CreateAccountHandler::onExecute() {
    assert(m_command);

    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    int64_t     fee{0};
    int64_t     deduct{0};
    uint32_t    msid;
    uint32_t    mpos;

    uint32_t newUser = 0;
    if (m_command->getDestBankId() == m_offi.svid) {
        newUser = m_offi.add_user(m_command->getBankId(), m_usera.pkey, m_command->getTime(), m_command->getUserId());
        if(!newUser) {
            errorCode = ErrorCodes::Code::eCreateAccountFail;
            DLOG("ERROR: failed to create account\n");
        } else {
            m_command->setNewUser(newUser, m_usera.pkey);
        }
    }

    if (!errorCode) {
        deduct = m_command->getDeduct();
        fee = m_command->getFee();

        //commit changes
        m_usera.msid++;
        m_usera.time=m_command->getTime();
        m_usera.lpath=lpath;
        m_usera.node = 0;
        m_usera.user = newUser;

        Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

        if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
            DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
            errorCode = ErrorCodes::Code::eMessageSubmitFail;
        } else {

            m_offi.set_user(m_command->getUserId(), m_usera, deduct+fee);

            log_t tlog;
            tlog.time   = time(NULL);
            tlog.type   = m_command->getType();
            tlog.node   = m_command->getBankId();
            tlog.user   = newUser;
            tlog.umid   = m_command->getUserMessageId();
            tlog.nmid   = msid;
            tlog.mpos   = mpos;
            tlog.weight = -deduct;
            m_offi.put_ulog(m_command->getUserId(), tlog);

            if (m_command->getBankId() == m_command->getDestBankId()) {
                tlog.type|=0x8000; //incoming
                tlog.node = m_command->getBankId();
                tlog.user = m_command->getUserId();
                tlog.weight = deduct;
                m_offi.put_ulog(newUser, tlog);
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

bool CreateAccountHandler::onValidate() {
    int64_t deduct = m_command->getDeduct();
    int64_t fee = m_command->getFee();
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    if(deduct+fee+(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight) {
        DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
             deduct, fee, (uint64_t)(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
        errorCode = ErrorCodes::Code::eLowBalance;
    }
    else if (!m_offi.svid || m_command->getBankId()!= m_offi.svid) {
        DLOG("ERROR Incorrect bank: %d, should be %d\n", m_command->getBankId(), m_offi.svid);
        errorCode = ErrorCodes::Code::eBankIncorrect;
    }
    else if(m_offi.readonly) { //FIXME, notify user.cpp about errors !!!
        DLOG("OFFICE: reject transaction in readonly mode (todo: add notification)\n");
        errorCode = ErrorCodes::Code::eReadOnlyMode;
    }
    else if(m_usera.msid!=m_command->getUserMessageId()) {
        DLOG("ERROR: bad msid %08X<>%08X\n", m_usera.msid, m_command->getUserMessageId());
        errorCode = ErrorCodes::Code::eBadMsgId;
    }
    else if (m_command->getDestBankId() != m_offi.svid) {
        uint32_t now = time(NULL);
        if(now%BLOCKSEC>BLOCKSEC/2) {
            DLOG("ERROR: bad timing for remote account request, try after %d seconds\n",
                 BLOCKSEC-now%BLOCKSEC);
            errorCode = ErrorCodes::Code::eCreateAccountBadTiming;
        }
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
