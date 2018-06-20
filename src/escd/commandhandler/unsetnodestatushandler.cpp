#include "unsetnodestatushandler.h"
#include "command/unsetnodestatus.h"
#include "../office.hpp"
#include "helper/hash.h"

UnsetNodeStatusHandler::UnsetNodeStatusHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void UnsetNodeStatusHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<UnsetNodeStatus>(dynamic_cast<UnsetNodeStatus*>(command.release()));
    } catch (std::bad_cast& bc) {
        ELOG("UnsetNodeStatus bad_cast caught: %s\n", bc.what());
        return;
    }
}

void UnsetNodeStatusHandler::onExecute() {
    assert(m_command);
    const auto res = commitChanges(*m_command);

    if(!res.errorCode) {
        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getDestBankId();
        tlog.user   = 0;
        tlog.umid   = m_command->getUserMessageId();
        tlog.nmid   = res.msid;
        tlog.mpos   = res.mpos;
        tlog.weight = 0;

        tInfo info;
        info.weight = m_command->getStatus();
        info.deduct = m_command->getDeduct();
        info.fee = m_command->getFee();
        info.stat = m_usera.stat;
        memcpy(info.pkey, m_usera.pkey, sizeof(info.pkey));
        memcpy(tlog.info, &info, sizeof(tInfo));

        m_offi.put_ulog(m_command->getUserId(), tlog);
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&res.errorCode, ERROR_CODE_LENGTH));
        if(!res.errorCode) {
            commandresponse response{m_usera, res.msid, res.mpos};
            boost::asio::write(m_socket, boost::asio::buffer(&response, sizeof(response)));
        }
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code UnsetNodeStatusHandler::onValidate() {
    if(!m_offi.check_user(m_command->getDestBankId(), 0)) {
        DLOG("ERROR: bad target node %04X\n", m_command->getDestBankId());
        return ErrorCodes::Code::eNodeBadTarget;
    }

    if(m_command->getUserId()) {
        DLOG("ERROR: user %08X not authorized to change node status bits\n",
            m_command->getUserId());
        return ErrorCodes::Code::eNoNodeStatusChangeAuth;
    }

    if(0x0 != (m_command->getStatus()&0x7)) {
        DLOG("ERROR: not authorized to change first three bits of node status for node %04X\n",
             m_command->getDestBankId());
        return ErrorCodes::Code::eAuthorizationError;
    }

    return ErrorCodes::Code::eNone;
}

