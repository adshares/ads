#include "retrievefundshandler.h"
#include "command/retrievefunds.h"
#include "../office.hpp"
#include "helper/hash.h"

RetrieveFundsHandler::RetrieveFundsHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void RetrieveFundsHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<RetrieveFunds>(std::move(command));
}

void RetrieveFundsHandler::onExecute() {
    assert(m_command);
    const auto res = commitChanges(*m_command);

    if(!res.errorCode) {
        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getDestBankId();
        tlog.user   = m_command->getDestUserId();
        tlog.umid   = m_command->getUserMessageId();
        tlog.nmid   = res.msid;
        tlog.mpos   = res.mpos;

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
        boost::asio::write(m_socket, boost::asio::buffer(&res.errorCode, ERROR_CODE_LENGTH));
        if(!res.errorCode) {
            commandresponse response{m_usera, res.msid, res.mpos};
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

