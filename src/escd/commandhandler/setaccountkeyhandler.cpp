#include "setaccountkeyhandler.h"
#include "command/setaccountkey.h"
#include "../office.hpp"
#include "helper/hash.h"

SetAccountKeyHandler::SetAccountKeyHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void SetAccountKeyHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<SetAccountKey>(dynamic_cast<SetAccountKey*>(command.release()));
    } catch (std::bad_cast& bc) {
        ELOG("OnSetAccountKey bad_cast caught: %s\n", bc.what());
        return;
    }
}

void SetAccountKeyHandler::onExecute() {
    assert(m_command);

    accountkey& data            = m_command->getDataStruct();
    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    uint32_t    msid;
    uint32_t    mpos;
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    //execute
    std::copy(data.pubkey, data.pubkey + SHA256_DIGEST_LENGTH, m_usera.pkey);

    m_usera.msid++;
    m_usera.time  = data.ttime;
    m_usera.lpath = lpath;
    //convert message to hash (use signature as input)
    Helper::create256signhash(data.sign, m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

    // could add set_user here
    if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
        ELOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        errorCode = ErrorCodes::Code::eMessageSubmitFail;
    } else {
        m_offi.set_user(data.auser, m_usera, m_command->getDeduct()+m_command->getFee()); //will fail if status changed !!!

        //addlogs
        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = data.abank;
        tlog.user   = data.auser;
        tlog.umid   = data.amsid;
        tlog.nmid   = msid;
        tlog.mpos   = mpos;

        tlog.weight = -m_command->getDeduct() - m_command->getFee();
        m_offi.put_ulog(data.auser, tlog);
    }

#ifdef DEBUG
    DLOG("SENDING new user info %04X:%08X @ msg %08X:%08X\n", m_usera.node, m_usera.user, msid, mpos);
#endif

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            commandresponse response{m_usera, msid, mpos};
            boost::asio::write(m_socket, boost::asio::buffer(&response, sizeof(response)));
        }
    } catch (std::exception& e) {
        ELOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

bool SetAccountKeyHandler::onValidate() {
    int64_t     deduct = m_command->getDeduct();
    int64_t     fee    = m_command->getFee();
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    if(deduct+fee+(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight) {
        ELOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
             deduct, fee, (uint64_t)(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
            errorCode = ErrorCodes::Code::eLowBalance;
    }

    if (errorCode) {
        try {
            boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        } catch (std::exception& e) {
            ELOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
        }
        return false;
    }

    return true;
}
