#include "setaccountkeyhandler.h"
#include "command/setaccountkey.h"
#include "../office.hpp"
#include "helper/hash.h"

SetAccountKeyHandler::SetAccountKeyHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket)
{
}

void SetAccountKeyHandler::onInit(std::unique_ptr<IBlockCommand> command)
{
    try{
        m_command = std::unique_ptr<SetAccountKey>(dynamic_cast<SetAccountKey*>(command.release()));
    }
    catch (std::bad_cast& bc){
        std::cerr << "OnSetAccountKey bad_cast caught: " << bc.what() << '\n';
        return;
    }
}

void SetAccountKeyHandler::onExecute()
{
    assert(m_command);

    accountkey& data            = m_command->getDataStruct();
    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    int64_t     deduct{0};
    int64_t     fee{TXS_KEY_FEE};

    //execute
    std::copy(data._pubkey, data._pubkey + SHA256_DIGEST_LENGTH, m_usera.pkey);

    m_usera.msid++;
    m_usera.time  = data._ttime;
    m_usera.lpath = lpath;
    //convert message to hash (use signature as input)
    Helper::create256signhash(data._sign, SHA256_DIGEST_LENGTH, m_usera.hash, m_usera.hash);

    //commit changes
    uint32_t msid;
    uint32_t mpos;

    // could add set_user here
    if(!m_offi.add_msg(*m_command.get(), msid, mpos))
    {
        DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
        return;
    }

    m_offi.set_user(data._auser, m_usera, deduct+fee); //will fail if status changed !!!

    //addlogs
    log_t tlog;
    tlog.time   = time(NULL);
    tlog.type   = m_command->getType();
    tlog.node   = data._abank;
    tlog.user   = data._auser;
    tlog.umid   = data._amsid;
    tlog.nmid   = msid;
    tlog.mpos   = mpos;

    tlog.weight = -deduct;
    m_offi.put_ulog(data._auser, tlog);

#ifdef DEBUG
    DLOG("SENDING new user info %04X:%08X @ msg %08X:%08X\n", m_usera.node, m_usera.user, msid, mpos);
#endif

    //send response
    try
    {
      commandresponse response{ m_usera, msid, mpos};
      boost::asio::write(m_socket, boost::asio::buffer(&response, sizeof(response))); //consider signing this message
    }
    catch (std::exception& e){
      DLOG("ERROR responding to client %08X\n",m_usera.user);
    }
}

bool SetAccountKeyHandler::onValidate()
{
    int64_t     deduct{0};
    int64_t     fee{TXS_KEY_FEE};
    accountkey& data = m_command->getDataStruct();

    if(deduct+fee+(data._auser ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight)
    {
        DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
        deduct, fee, (uint64_t)(data._auser ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
        return false;
    }

    return true;
}
