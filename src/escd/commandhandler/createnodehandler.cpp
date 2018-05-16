#include "createnodehandler.h"
#include "command/createnode.h"
#include "../office.hpp"
#include "helper/hash.h"

CreateNodeHandler::CreateNodeHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void CreateNodeHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<CreateNode>(dynamic_cast<CreateNode*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("OnCreateNode bad_cast caught: %s\n", bc.what());
        return;
    }
}

void CreateNodeHandler::onExecute() {
    assert(m_command);

    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    m_usera.msid++;
    m_usera.time  = m_command->getTime();
    //m_usera.node = 0;
    //m_usera.user = 0;
    m_usera.lpath = lpath;
    //convert message to hash (use signature as input)
    Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

    //commit changes
    uint32_t msid;
    uint32_t mpos;

    // could add set_user here
    if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
        ELOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        errorCode = ErrorCodes::Code::eMessageSubmitFail;
    } else {

        m_offi.set_user(m_command->getUserId(), m_usera, m_command->getDeduct()+m_command->getFee()); //will fail if status changed !!!

        //addlogs
        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getBankId();
        tlog.user   = m_command->getUserId();
        tlog.umid   = m_command->getMessageId();
        tlog.nmid   = msid;
        tlog.mpos   = mpos;

        tInfo info;
        info.weight = m_usera.weight;
        info.deduct = m_command->getDeduct();
        info.fee = m_command->getFee();
        info.stat = m_usera.stat;
        memcpy(info.pkey, m_usera.pkey, sizeof(info.pkey));
        memcpy(tlog.info, &info, sizeof(tInfo));

        tlog.weight = -m_command->getDeduct();
        m_offi.put_ulog(m_command->getUserId(), tlog);

#ifdef DEBUG
    DLOG("SENDING new user info %04X:%08X @ msg %08X:%08X\n", m_usera.node, m_usera.user, msid, mpos);
#endif
    }

    //send response
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

bool CreateNodeHandler::onValidate() {
    int64_t     deduct = m_command->getDeduct();
    int64_t     fee    = m_command->getFee();
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    if (m_command->getBankId() != m_offi.svid) {
        ELOG("ERROR: bad bank\n");
	errorCode = ErrorCodes::Code::eBadNode;
    } else if(m_offi.readonly) { //FIXME, notify user.cpp about errors !!!
        ELOG("OFFICE: reject transaction in readonly mode (todo: add notification)\n");
	errorCode = ErrorCodes::Code::eReadOnlyMode;
    } else if(m_usera.msid!=m_command->getMessageId()) {
        ELOG("ERROR: bad msid %08X<>%08X\n",m_usera.msid, m_command->getMessageId());
	errorCode = ErrorCodes::Code::eBadMsgId;
    } else if(deduct+fee+(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight) {
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
