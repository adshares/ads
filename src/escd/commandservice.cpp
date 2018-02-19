#include "commandservice.h"
#include "command/getaccount.h"
#include "command/setaccountkey.h"
#include "office.hpp"

CommandService::CommandService(office& office, boost::asio::ip::tcp::socket& socket)
    : m_offi(office),
      m_socket(socket)
{

}

void CommandService::onExecute(std::unique_ptr<IBlockCommand> command)
{
    int type = command->getType();

    switch(type)
    {
        case TXSTYPE_INF:
            onGetAccount(command);
            break;
        case TXSTYPE_KEY:
            onSetAccountKey(command);
            break;
        default:
            break;
    }
}

void CommandService::onGetAccount(std::unique_ptr<IBlockCommand>& command)
{
    auto getAccount     = dynamic_cast<GetAccount*>(command.get());
    userinfo&   data    = getAccount->getDataStruct()._info;
    int32_t     diff    = data._ttime - time(nullptr);

    // this is special, just local info
    if((abs(diff)>22))
    {
        DLOG("ERROR: high time difference (%d>2s)\n",diff);
        return;
    }

//FIXME, read data also from server
//FIXME, if local account locked, check if unlock was successfull based on time passed after change
  if(data._abank != m_offi.svid && data._bbank != m_offi.svid)
  {
    DLOG("ERROR: bad bank for INF\n");
    return;
  }
#ifdef DEBUG
  DLOG("SENDING user info %08X:%04X\n", data._bbank, data._buser);
#endif

  user_t usera;
  user_t userb;

  if(!m_offi.get_user(usera, data._abank, data._auser))
  {
      DLOG("ERROR: read user failed\n");
      return;
  }

  if(data._abank != data._bbank || data._auser != data._buser)
  {
    if(!m_offi.get_user(userb, data._bbank, data._buser))
    {
      DLOG("FAILED to get user info %08X:%04X\n", data._bbank, data._buser);
      return;
    }
    boost::asio::write(m_socket, boost::asio::buffer(&userb, sizeof(user_t)));
  }
  else
  {
    boost::asio::write(m_socket,boost::asio::buffer(&usera, sizeof(user_t)));
  }

  //TODO Check why two times we are sending data.
  if(data._bbank)
  {
    if(!m_offi.get_user_global(userb, data._bbank, data._buser))
    {
      DLOG("FAILED to get global user info %08X:%04X\n", data._bbank, data._buser);
      return;
    }
    //boost::asio::write(m_socket,boost::asio::buffer(&userb, sizeof(user_t)));
  }

}

void CommandService::onSetAccountKey(std::unique_ptr<IBlockCommand>& command)
{
    auto        setKeyAccount   = dynamic_cast<SetAccountKey*>(command.get());
    accountkey& data            = setKeyAccount->getDataStruct();
    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;

    int64_t     deduct{0};
    int64_t     fee{TXS_KEY_FEE};

    user_t usera;
    //user_t userb;

    if(!m_offi.get_user(usera, data._abank, data._auser)){
        DLOG("ERROR: read user failed\n");
        return;
    }

    memcpy(usera.pkey, data._pubkey ,32);

    //commit changes
    usera.msid++;
    usera.time  = data._ttime;
    //usera.node  = lnode;
    //usera.user  = luser;
    usera.lpath = lpath;
    //convert message to hash (use signature as input)
    uint8_t     hash[32];
    SHA256_CTX  sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data._pubkeysign,64);
    SHA256_Final(hash,&sha256);
    SHA256_Init(&sha256);
    SHA256_Update(&sha256,usera.hash,32);
    SHA256_Update(&sha256,hash,32);
    SHA256_Final(usera.hash,&sha256);

    uint32_t msid;
    uint32_t mpos;

    m_offi.add_msg(command, msid, mpos); // could add set_user here

    //if(!msid||!m_mpos){
    if(!msid)
    {
      DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
      return;
    }

    if(deduct+fee+(data._auser?USER_MIN_MASS:BANK_MIN_UMASS)>usera.weight)
    {
      DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
        deduct,fee,(uint64_t)(data._auser?USER_MIN_MASS:BANK_MIN_UMASS),usera.weight);
      return;
    }

    m_offi.set_user(data._auser, usera, deduct+fee); //will fail if status changed !!!

    log_t tlog;
    tlog.time   = time(NULL);
    tlog.type   = command->getType();
    tlog.node   = data._abank;
    tlog.user   = data._auser;
    tlog.umid   = data._amsid;
    tlog.nmid   = msid;
    tlog.mpos   = mpos;

    tlog.weight =-deduct;
    m_offi.put_ulog(data._auser, tlog);

#ifdef DEBUG
    DLOG("SENDING new user info %04X:%08X @ msg %08X:%08X\n", usera.node, usera.user, msid, mpos);
#endif

    try{
      //FIXME, respond with a single message
      boost::asio::write(m_socket,boost::asio::buffer(&usera, sizeof(user_t))); //consider signing this message
      boost::asio::write(m_socket,boost::asio::buffer(&msid, sizeof(uint32_t)));
      boost::asio::write(m_socket,boost::asio::buffer(&mpos, sizeof(uint32_t)));}
    catch (std::exception& e){
      DLOG("ERROR responding to client %08X\n",usera.user);
    }
}
