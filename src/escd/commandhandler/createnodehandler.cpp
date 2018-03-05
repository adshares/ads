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
        std::cerr << "OnCreateNode bad_cast caught: " << bc.what() << '\n';
        return;
    }
}

void CreateNodeHandler::onExecute() {
    assert(m_command);

    createnodedata& data            = m_command->getDataStruct();
    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;

    //execute
    //std::copy(data.pubkey, data.pubkey + SHA256_DIGEST_LENGTH, m_usera.pkey);

    m_usera.msid++;
    m_usera.time  = data.ttime;
    m_usera.lpath = lpath;
    //convert message to hash (use signature as input)
    Helper::create256signhash(data.sign, SHA256_DIGEST_LENGTH, m_usera.hash, m_usera.hash);

    //commit changes
    uint32_t msid;
    uint32_t mpos;

    // could add set_user here
    if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
        DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        return;
    }

    m_offi.set_user(data.auser, m_usera, m_command->getDeduct()+m_command->getFee()); //will fail if status changed !!!

    //addlogs
    log_t tlog;
    tlog.time   = time(NULL);
    tlog.type   = m_command->getType();
    tlog.node   = data.abank;
    tlog.user   = data.auser;
    tlog.umid   = data.amsid;
    tlog.nmid   = msid;
    tlog.mpos   = mpos;

    tlog.weight = -m_command->getDeduct() - m_command->getFee();
    m_offi.put_ulog(data.auser, tlog);

#ifdef DEBUG
    DLOG("SENDING new user info %04X:%08X @ msg %08X:%08X\n", m_usera.node, m_usera.user, msid, mpos);
#endif

    //send response
    try {
        commandresponse response{ m_usera, msid, mpos};
        boost::asio::write(m_socket, boost::asio::buffer(&response, sizeof(response))); //consider signing this message
    } catch (std::exception& e) {
        DLOG("ERROR responding to client %08X\n",m_usera.user);
    }
}

bool CreateNodeHandler::onValidate() {
    int64_t     deduct = m_command->getDeduct();
    int64_t     fee    = m_command->getFee();

    if(deduct+fee+(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight) {
        DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
             deduct, fee, (uint64_t)(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
        return false;
    }

    return true;
}
