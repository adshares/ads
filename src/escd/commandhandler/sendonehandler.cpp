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

    int64_t deposit = m_command->getDeduct(); // if deposit==0 inform target
    const auto res = commitChanges(*m_command);

    if (!res.errorCode) {
        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getDestBankId();
        tlog.user   = m_command->getDestUserId();
        tlog.umid   = m_command->getUserMessageId();
        tlog.nmid   = res.msid;
        tlog.mpos   = res.mpos;
        tlog.weight = -m_command->getDeduct();
        memcpy(tlog.info, m_command->getInfoMsg(),32);
        m_offi.put_ulog(m_command->getUserId(),  tlog);

        if (m_command->getBankId() == m_command->getDestBankId()) {
            tlog.type|=0x8000; //incoming
            tlog.node=m_command->getBankId();
            tlog.user=m_command->getUserId();
            tlog.weight=m_command->getDeduct();
            m_offi.put_ulog(m_command->getDestUserId(), tlog);
            if(deposit>=0) {
                m_offi.add_deposit(m_command->getDestUserId(), deposit);
            }
        }
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

ErrorCodes::Code SendOneHandler::onValidate() {
    if(!m_offi.check_user(m_command->getDestBankId(), m_command->getDestUserId())) {
        DLOG("ERROR: bad target: node %04X user %04X\n", m_command->getDestBankId(), m_command->getDestUserId());
        return ErrorCodes::Code::eUserBadTarget;
    }
    return ErrorCodes::Code::eNone;
}


