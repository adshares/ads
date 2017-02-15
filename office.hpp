#ifndef OFFICE_HPP
#define OFFICE_HPP

#include "main.hpp"

class client;
typedef boost::shared_ptr<client> client_ptr;

//before optimising remember that the required througput here is much lower than the throughput of the server
//consider running the processing in a single thread

class office
{
public:
  office(boost::asio::io_service& io_service,const boost::asio::ip::tcp::endpoint& endpoint,options& opts,server& srv) :
    io_service_(io_service),
    acceptor_(io_service, endpoint),
    srv_(srv),
    opts_(opts),
    fd(0),
    message_sent(0)
  { svid=opts_.svid,
    users=srv_.last_srvs_.nodes[svid].users; //FIXME !!! this maybe incorrect !!!
    memcpy(pkey,srv_.pkey,32);
    deposit.resize(users), // a buffer to enable easy resizing of the account vector
    std::cerr<<"OFFICE ("<<svid<<") open\n";
    // prapare local register [this could be in RAM later or be on a RAM disk]
    mkdir("ofi",0755);
    char filename[64];
    sprintf(filename,"ofi/%04X.dat",svid);
    fd=open(filename,O_RDWR|O_CREAT|O_TRUNC,0644); // truncate to force load from main repository
    if(fd<0){
      std::cerr<<"ERROR, failed to open office register\n";}
    msid=srv_.msid_;


    run=true;
    start_accept();
    clock_thread = new boost::thread(boost::bind(&office::clock, this));
  }

  ~office()
  { if(fd){
      close(fd);}
  }

  void stop() // send and empty txs queue
  { run=false;
    clock_thread->interrupt();
    clock_thread->join();
    //submit message
  }

  void clock()
  { while(run){
      uint32_t now=time(NULL);
//FIXME, do not submit messages in vulnerable time (from blockend-margin to new block confirmation)
      //TODO, clear hanging clients
      boost::this_thread::sleep(boost::posix_time::seconds(2));
      if(message.empty()){
        message_.lock();
        srv_.break_silence(now,message);
        message_.unlock();
        continue;}
      if(message.length()<MESSAGE_LEN_OK && message_sent+MESSAGE_WAIT>now){
	std::cerr<<"WARNING, waiting for more messages\n";
        continue;}
      if(!srv_.accept_message(msid)){
	std::cerr<<"WARNING, server not ready for a new message ("<<msid<<")\n";
        continue;}
      message_.lock();
      std::cerr<<"SENDING new message (old msid:"<<msid<<")\n";
      msid=srv_.write_message(message);
      message.clear();
      message_.unlock();
      message_sent=now;}
  }

  bool check_account(uint16_t cbank,uint32_t cuser)
  { if(cbank==svid && cuser<users){
      return(true);}
    if(srv_.last_srvs_.nodes.size()<=cbank){
      return(false);}
    if(srv_.last_srvs_.nodes[cbank].users<=cuser){
      return(false);}
    return(true);
  }

  bool get_user(user_t& u,uint16_t cbank,uint32_t cuser,bool global)
  { u.msid=0;
    if(cuser>=users){
      return(false);}
    if(cbank==svid && !global){
      file_.lock();
      lseek(fd,cuser*sizeof(user_t),SEEK_SET);
      read(fd,&u,sizeof(user_t));}
    if(!u.msid){
      srv_.last_srvs_.get_user(u,cbank,cuser);} //watch out for deadlocks
    if(!u.msid){
      file_.unlock();
      return(false);}
    if(cbank==svid && deposit[cuser]){
      u.weight+=deposit[cuser];
      deposit[cuser]=0;
      assert(u.weight>=0);
      lseek(fd,cuser*sizeof(user_t),SEEK_SET);
      write(fd,&u,sizeof(user_t));}
    file_.unlock();
    return(true);
  }

  uint32_t add_user(uint16_t abank,uint8_t* pk,uint32_t when) // will create new account or overwrite old one
  { static uint32_t nuser=0;
    static uint32_t lastnow=0;
    uint32_t now=srv_.last_srvs_.now;
    user_t nu;
    char filename[64];
    sprintf(filename,"ofi/%04X.dat",svid);
    int nd=open(filename,O_RDONLY);
    if(nd<0){
      std::cerr<<"ERROR, failed to open office register\n";
      return(0);}
    if(lastnow!=now){
      lastnow=now;
      nuser=1;}
    else{
      nuser++;}
    if(lseek(nd,nuser*sizeof(user_t),SEEK_SET)!=(int64_t)nuser*(int64_t)sizeof(user_t)){
      fprintf(stderr,"ERROR seeking user %08X in office\n",nuser);
      exit(-1);} //FIXME, do not exit
    for(;nuser<users;nuser++){ // try overwriting old dead account
      if(read(nd,&nu,sizeof(user_t))!=sizeof(user_t) || !nu.msid){
        fprintf(stderr,"ERROR reading user %08X (users:%08X) in office %s\n",nuser,users,filename);
        exit(-1);} //FIXME, do not exit
      if(now>nu.lpath+LOCK_TIME && nu.weight-TIME_FEE(now,nu.lpath)<0){ // try changing this account
//FIXME, do not change accounts that are open for too short
      //if(nu.status & USER_CLOSED){ // try changing this account
        file_.lock();
        lseek(fd,nuser*sizeof(user_t),SEEK_SET);
        read(fd,&nu,sizeof(user_t));
//FIXME !!!
//FIXME, network conflict ... somebody can wire funds to this account while the account is beeing overwritten
//FIXME !!! (maybe create a slow 'account close' transaction)
        if(nu.weight-TIME_FEE(now,nu.lpath)<0){ // commit changing this account
//FIXME, do not change accounts that are open for too short
          //std::cerr<<"WARNING, overwriting account "<<nuser<<"\n";
          fprintf(stderr,"WARNING, overwriting account %08X [weight:%016lX fee:%08X]\n",
            nuser,nu.weight,TIME_FEE(now,nu.lpath));
          //FIXME !!!  wrong time !!! must use time from txs
          srv_.last_srvs_.init_user(nu,svid,nuser,(abank==svid?MIN_MASS:0),pk,when);
          //memset(&nu,0,sizeof(user_t));
          //memset(&nu.hash,0xff,SHA256_DIGEST_LENGTH);
          //memcpy(nu.hash,&nuser,4); // always start with a unique hash
          //memcpy(nu.hash+4,&svid,2); // always start with a unique hash
          //nu.msid=1;
          //nu.time=now;
          //nu.node=svid;
          //nu.user=nuser; // record user_id
          //nu.lpath=now;
          //nu.rpath=now-START_AGE;
          //nu.weight=(abank==svid?MIN_MASS:0); // deposit funds imediately if local transaction
          //memcpy(nu.pkey,pk,SHA256_DIGEST_LENGTH);
          lseek(fd,-sizeof(user_t),SEEK_CUR);
          write(fd,&nu,sizeof(user_t));
          file_.unlock();
          close(nd);
          return(nuser);}
        file_.unlock();}}
    close(nd);
    if(users>=MAX_USERS){
      return(0);}
    // no old account found, creating new account
    //FIXME !!!  wrong time !!! must use time from txs
    srv_.last_srvs_.init_user(nu,svid,nuser,(abank==svid?MIN_MASS:0),pk,when);
    //memset(&nu,0,sizeof(user_t));
    //memset(&nu.hash,0xff,SHA256_DIGEST_LENGTH);
    //memcpy(nu.hash,&nuser,4); // always start with a unique hash
    //memcpy(nu.hash+4,&svid,2); // always start with a unique hash
    //nu.msid=1;
    //nu.time=now;
    //nu.node=svid;
    //nu.user=nuser; // record user_id
    //nu.lpath=now;
    //nu.rpath=now-START_AGE;
    //nu.weight=(abank==svid?MIN_MASS:0); // deposit funds imediately if local transaction
    //memcpy(nu.pkey,pk,SHA256_DIGEST_LENGTH);
    std::cerr<<"CREATING new account "<<nuser<<"\n";
    file_.lock();
    if(users>=MAX_USERS){
      file_.unlock();
      return(0);}
    nuser=users++;
    deposit.push_back(0);
    lseek(fd,nuser*sizeof(user_t),SEEK_SET);
    write(fd,&nu,sizeof(user_t));
    file_.unlock();
    return(nuser);
  }

  void set_user(uint32_t user,user_t& u, int64_t deduct)
  { assert(user<users);
    file_.lock();
    u.weight+=deposit[user]-deduct;
    deposit[user]=0;
    assert(u.weight>=0);
    lseek(fd,user*sizeof(user_t),SEEK_SET);
    write(fd,&u,sizeof(user_t));
    file_.unlock();
  }

  void add_deposit(usertxs& utxs)
  { file_.lock();
    deposit[utxs.buser]+=utxs.tmass;
    file_.unlock();
    //FIXME, save in local transaction history
    //save in loc/XX/XX/XX/XX.dat

    //FIXME, process contracts if needed
  }

  bool try_account(hash_s* key)
  { account_.lock();
    if(accounts_.size()>MAX_ACCOUNT){
      for(auto it=accounts_.begin();it!=accounts_.end();){
        auto jt=it++;
        user_t u;
        get_user(u,svid,jt->second,false); //watch out for deadlocks
        if(u.weight>=MIN_MASS){
          accounts_.erase(jt);}}
      if(accounts_.size()>MAX_ACCOUNT){
        account_.unlock();
        return(false);}}
    if(accounts_.find(*key)!=accounts_.end()){
      account_.unlock();
      return(false);}
    account_.unlock();
    return(true);
  }

  void add_account(hash_s* key,uint32_t user)
  { account_.lock();
    accounts_[*key]=user;
    account_.unlock();
  }

  void add_msg(uint8_t* msg,int len, int64_t fee)
  { if(!run){ return;}
    message_.lock();
    message.append((char*)msg,len);
    message_.unlock();
    mfee+=BANK_PROFIT(fee); //do we need this?
    //mfee should be commited to bank from time to time.
  }

  void lock_user(uint32_t cuser)
  { users_[cuser & 0xff].lock();
  }

  void unlock_user(uint32_t cuser)
  { users_[cuser & 0xff].unlock();
  }

  void start_accept(); // main.cpp
  void handle_accept(client_ptr c,const boost::system::error_code& error); // main.cpp, currently blocking :-(
  //void leave(client_ptr c); // main.cpp

  //uint8_t* pkey()
  //{ return(opts_.pk);
  //}

  //void get_user(user_t& u,uint16_t peer,uint32_t uid)
  //{ return(srv_.srvs_.get_user(u,peer,uid);
  //}

  bool check_user(uint16_t peer,uint32_t uid)
  { if(peer!=svid){
      return(srv_.last_srvs_.check_user(peer,uid));} //use last_srvs_ for safety
    return(uid<=users);
  }

  bool get_log(uint16_t svid,uint32_t user,uint32_t from,std::string& slog)
  { return(srv_.get_log(svid,user,from,slog));
  }

  bool find_key(uint8_t* pkey,uint8_t* skey)
  { return(srv_.last_srvs_.find_key(pkey,skey));
  }

  uint32_t path()
  { return(srv_.last_srvs_.now);
  }

  bool run;
  uint16_t svid;
  uint32_t users; //number of users of the bank
  std::vector<int64_t> deposit; //resizing will require a stop of processing
  boost::mutex users_[0x100];
  std::string message;
  boost::mutex message_;
  hash_t pkey; // local copy for managing updates
private:
  boost::asio::io_service& io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
  server& srv_;
  options& opts_;
  std::set<client_ptr> clients_;
  boost::mutex client_;
  int fd; // user file descriptor
  boost::mutex file_;
  uint32_t msid;
   int64_t mfee; // let's see if we need this
  uint32_t message_sent;
  boost::thread* clock_thread;
  std::map<hash_s,uint32_t,hash_cmp> accounts_; // list of candidates
  boost::mutex account_;
};

#endif // OFFICE_HPP
