#include "retrievefundshandler.h"
#include "command/retrievefunds.h"
#include "../office.hpp"
#include "helper/hash.h"

RetrieveFundsHandler::RetrieveFundsHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void RetrieveFundsHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<RetrieveFunds>(dynamic_cast<RetrieveFunds*>(command.release()));
    } catch (std::bad_cast& bc) {
        ELOG("RetrieveFunds bad_cast caught: %s\n", bc.what());
        return;
    }
}

void RetrieveFundsHandler::onExecute() {
    assert(m_command);

    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;

    //commit changes
    m_usera.msid++;
    m_usera.time  = m_command->getTime();
    m_usera.lpath = lpath;

    Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

    uint32_t msid;
    uint32_t mpos;

    if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
        ELOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        errorCode = ErrorCodes::Code::eMessageSubmitFail;
    }

    if(!errorCode) {
        m_offi.set_user(m_command->getUserId(), m_usera, m_command->getDeduct()+m_command->getFee());

        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getDestBankId();
        tlog.user   = m_command->getDestUserId();
        tlog.umid   = m_command->getUserMessageId();
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

ErrorCodes::Code RetrieveFundsHandler::onValidate() {
    if(m_command->getBankId() == m_command->getDestBankId()) {
        DLOG("ERROR: bad bank %04X, use PUT\n", m_command->getDestBankId());
        return ErrorCodes::Code::eBankIncorrect;
    }
    return ErrorCodes::Code::eNone;
}

