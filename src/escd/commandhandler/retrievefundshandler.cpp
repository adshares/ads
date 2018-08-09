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
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&res.errorCode, ERROR_CODE_LENGTH));
        if(!res.errorCode) {
            response.emplace_back(boost::asio::buffer(&m_usera, sizeof(m_usera)));
            response.emplace_back(boost::asio::buffer(&res.msid, sizeof(res.msid)));
            response.emplace_back(boost::asio::buffer(&res.mpos, sizeof(res.mpos)));
        }
        boost::asio::write(m_socket, response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_command->getUserId(), e.what());
    }
}

void RetrieveFundsHandler::onValidate() {
    if(m_command->getBankId() == m_command->getDestBankId()) {
        DLOG("ERROR: bad bank %04X, use PUT\n", m_command->getDestBankId());
        throw ErrorCodes::Code::eBankIncorrect;
    }
    if(!m_offi.check_user(m_command->getDestBankId(), m_command->getDestUserId())) {
        DLOG("ERROR: bad target: node %04X user %04X\n", m_command->getDestBankId(), m_command->getDestUserId());
        throw ErrorCodes::Code::eUserBadTarget;
    }
}

