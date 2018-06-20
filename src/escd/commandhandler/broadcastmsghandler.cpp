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

    const auto res = commitChanges(*m_command);

    if (!res.errorCode) {
        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getAdditionalDataSize();
        tlog.user   = m_command->getUserId();
        tlog.umid   = m_command->getUserMessageId();
        tlog.nmid   = res.msid;
        tlog.mpos   = res.mpos;
        tlog.weight = -m_command->getDeduct();

        tInfo info;
        info.weight = m_usera.weight;
        info.deduct = m_command->getDeduct();
        info.fee = m_command->getFee();
        info.stat = m_usera.stat;
        memcpy(info.pkey, m_usera.pkey, sizeof(info.pkey));
        memcpy(tlog.info, &info, sizeof(tInfo));

        m_offi.put_ulog(m_command->getUserId(),  tlog);
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&res.errorCode, ERROR_CODE_LENGTH));
        if(!res.errorCode) {
            commandresponse response{m_usera, res.msid, res.mpos};
            boost::asio::write(m_socket, boost::asio::buffer(&response, sizeof(response)));
        }
    } catch (std::exception&) {
        DLOG("ERROR responding to client %08X\n",m_usera.user);
    }
}

ErrorCodes::Code BroadcastMsgHandler::onValidate() {
    if (m_command->getAdditionalDataSize() > MAX_BROADCAST_LENGTH) {
        return ErrorCodes::Code::eBroadcastMaxLength;
    }
    return ErrorCodes::Code::eNone;
}

