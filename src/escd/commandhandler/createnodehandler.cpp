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

    //send response
    try {
        boost::asio::write(m_socket, boost::asio::buffer(&res.errorCode, ERROR_CODE_LENGTH));
        if(!res.errorCode) {
            commandresponse response{m_usera, res.msid, res.mpos};
            boost::asio::write(m_socket, boost::asio::buffer(&response, sizeof(response)));
        }
    } catch (std::exception& e) {
        ELOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code CreateNodeHandler::onValidate() {
    return ErrorCodes::Code::eNone;
}

