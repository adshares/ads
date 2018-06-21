#include "setaccountkeyhandler.h"
#include "command/setaccountkey.h"
#include "../office.hpp"
#include "helper/hash.h"

SetAccountKeyHandler::SetAccountKeyHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void SetAccountKeyHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<SetAccountKey>(std::move(command));
}

void SetAccountKeyHandler::onExecute() {
    assert(m_command);

    accountkey& data            = m_command->getDataStruct();
    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    uint32_t    msid;
    uint32_t    mpos;

    //execute    
    std::copy(data.pubkey, data.pubkey + SHA256_DIGEST_LENGTH, m_usera.pkey);

    m_usera.msid++;
    m_usera.time  = data.ttime;
    m_usera.lpath = lpath;
    //convert message to hash (use signature as input)
    Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

    auto errorCode = ErrorCodes::Code::eNone;
    // could add set_user here
    if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
        ELOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        errorCode = ErrorCodes::Code::eMessageSubmitFail;
    } else {
        m_offi.set_user(data.auser, m_usera, m_command->getDeduct()+m_command->getFee()); //will fail if status changed !!!

        //addlogs
        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = 0;
        tlog.user   = data.auser;
        tlog.umid   = data.amsid;
        tlog.nmid   = msid;
        tlog.mpos   = mpos;

        tInfo info;
        info.weight = m_usera.weight;
        info.deduct = m_command->getDeduct();
        info.fee = m_command->getFee();
        info.stat = m_usera.stat;
        memcpy(info.pkey, m_usera.pkey, sizeof(info.pkey));
        memcpy(tlog.info, &info, sizeof(tInfo));

        tlog.weight = -m_command->getDeduct() - m_command->getFee();
        m_offi.put_ulog(data.auser, tlog);
    }

#ifdef DEBUG
    DLOG("SENDING new user info %04X:%08X @ msg %08X:%08X\n", m_usera.node, m_usera.user, msid, mpos);
#endif

    try {
        std::vector<boost::asio::const_buffer> response;

        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));

        if(!errorCode) {            
            commandresponse cresponse{m_usera, msid, mpos};
            response.emplace_back(boost::asio::buffer(&cresponse, sizeof(cresponse)));
        }
        boost::asio::write(m_socket, response);

    } catch (std::exception& e) {
        ELOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code SetAccountKeyHandler::onValidate() {
    return ErrorCodes::Code::eNone;
}

