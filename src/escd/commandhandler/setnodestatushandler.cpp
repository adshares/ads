#include "setnodestatushandler.h"
#include "command/setnodestatus.h"
#include "../office.hpp"
#include "helper/hash.h"

SetNodeStatusHandler::SetNodeStatusHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void SetNodeStatusHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<SetNodeStatus>(std::move(command));
}

void SetNodeStatusHandler::onExecute() {
    assert(m_command);

    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    int64_t     fee             = m_command->getFee();
    int64_t     deduct          = m_command->getDeduct();

    //commit changes
    m_usera.msid++;
    m_usera.time  = m_command->getTime();
    m_usera.lpath = lpath;
    m_usera.node = 0;
    m_usera.user = 0;

    Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

    auto errorCode = ErrorCodes::Code::eNone;
    uint32_t msid, mpos;

    if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
        ELOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        errorCode = ErrorCodes::Code::eMessageSubmitFail;
    }

    if(!errorCode) {
        m_offi.set_user(m_command->getUserId(), m_usera, deduct+fee);

        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getDestBankId();
        tlog.user   = 0;
        tlog.umid   = m_command->getUserMessageId();
        tlog.nmid   = msid;
        tlog.mpos   = mpos;
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
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            response.emplace_back(boost::asio::buffer(&m_usera, sizeof(m_usera)));
            response.emplace_back(boost::asio::buffer(&msid, sizeof(msid)));
            response.emplace_back(boost::asio::buffer(&mpos, sizeof(mpos)));
        }
        boost::asio::write(m_socket, response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_command->getUserId(), e.what());
    }
}

void SetNodeStatusHandler::onValidate() {
    if(!m_offi.check_user(m_command->getDestBankId(), 0)) {
        DLOG("ERROR: bad target node %04X\n", m_command->getDestBankId());
        throw ErrorCodes::Code::eNodeBadTarget;
    }

    if(m_command->getUserId()) {
        DLOG("ERROR: user %08X not authorized to change node status bits\n",
            m_command->getUserId());
        throw ErrorCodes::Code::eNoNodeStatusChangeAuth;
    }

    if(0x0 != (m_command->getStatus()&0x7)) {
        DLOG("ERROR: not authorized to change first three bits of node status for node %04X\n",
             m_command->getDestBankId());
        throw ErrorCodes::Code::eAuthorizationError;
    }
}

