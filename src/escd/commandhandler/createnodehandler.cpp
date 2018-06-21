#include "createnodehandler.h"
#include "command/createnode.h"
#include "../office.hpp"
#include "helper/hash.h"

CreateNodeHandler::CreateNodeHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void CreateNodeHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<CreateNode>(std::move(command));
}

void CreateNodeHandler::onExecute() {
    assert(m_command);

    const auto res = commitChanges(*m_command);

    if(!res.errorCode) {
        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = 0;
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

#ifdef DEBUG
    DLOG("SENDING new user info %04X:%08X @ msg %08X:%08X\n", m_usera.node, m_usera.user, res.msid, res.mpos);
#endif
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
        ELOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code CreateNodeHandler::onValidate() {
    return ErrorCodes::Code::eNone;
}

