#include "broadcastmsghandler.h"
#include "command/broadcastmsg.h"
#include "../office.hpp"
#include "helper/hash.h"

BroadcastMsgHandler::BroadcastMsgHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}


void BroadcastMsgHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<BroadcastMsg>(dynamic_cast<BroadcastMsg*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("BroadcastMsg bad_cast caught: %s", bc.what());
        return;
    }

}

void BroadcastMsgHandler::onExecute() {
    assert(m_command);

    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    int64_t     fee{0};
    int64_t     deduct{0};

    deduct = m_command->getDeduct();
    fee = m_command->getFee();

    //commit changes
    m_usera.msid++;
    m_usera.time=m_command->getTime();
    m_usera.lpath=lpath;

    Helper::create256signhash(m_command->getSignature(), SHA256_DIGEST_LENGTH, m_usera.hash, m_usera.hash);

    uint32_t msid;
    uint32_t mpos;

    if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
        DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        return;
    }

    m_offi.set_user(m_command->getUserId(), m_usera, deduct+fee);

    log_t tlog;
    tlog.time   = time(NULL);
    tlog.type   = m_command->getType();
    tlog.node   = m_command->getAdditionalDataSize();
    tlog.user   = m_command->getUserId();
    tlog.umid   = m_command->getUserMessageId();
    tlog.nmid   = msid;
    tlog.mpos   = mpos;
    tlog.weight = -deduct;
    m_offi.put_ulog(m_command->getUserId(),  tlog);

    try {
        commandresponse response{m_usera, msid, mpos};
        boost::asio::write(m_socket, boost::asio::buffer(&response, sizeof(response)));
    } catch (std::exception&) {
        DLOG("ERROR responding to client %08X\n",m_usera.user);
    }
}



bool BroadcastMsgHandler::onValidate() {
    if(m_command->getBankId()!=m_offi.svid) {
        DLOG("ERROR: bad bank\n");
        return false;
    }
    if(!m_offi.svid) {
        DLOG("ERROR: svid = %d\n", m_offi.svid);
        return false;
    }

    if(m_offi.readonly) { //FIXME, notify user.cpp about errors !!!
        DLOG("OFFICE: reject transaction in readonly mode (todo: add notification)\n");
        return false;
    }

    if(m_usera.msid != m_command->getUserMessageId()) {
        DLOG("ERROR: bad msid %08X<>%08X\n", m_usera.msid, m_command->getUserMessageId());
        return false;
    }

    int64_t deduct = m_command->getDeduct();
    int64_t fee = m_command->getFee();
    if(deduct+fee+(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight) {
        DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
             deduct, fee, (uint64_t)(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
        return false;
    }

    return true;
}
