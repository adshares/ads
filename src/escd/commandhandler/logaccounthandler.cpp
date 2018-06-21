#include "logaccounthandler.h"
#include "command/logaccount.h"
#include "../office.hpp"
#include "helper/hash.h"

LogAccountHandler::LogAccountHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void LogAccountHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    IBlockCommand* blkCmd = command.release();
    m_command = std::unique_ptr<LogAccount>(dynamic_cast<LogAccount*>(blkCmd));
    if(!m_command) {
        ELOG("LogAccount ERROR while downcasting");
        delete blkCmd;
    }
}

void LogAccountHandler::onExecute() {
    assert(m_command);

    const auto res = commitChanges(*m_command);

    if(!res.errorCode) {
        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getBankId();
        tlog.user   = m_command->getUserId();
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
            commandresponse cresponse{m_usera, res.msid, res.mpos};
            response.emplace_back(boost::asio::buffer(&cresponse, sizeof(cresponse)));
        }
        boost::asio::write(m_socket, response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code LogAccountHandler::onValidate() {
    return ErrorCodes::Code::eNone;
}

