#ifndef OFFICE_HPP
#define OFFICE_HPP

class client;
typedef boost::shared_ptr<client> client_ptr;

//before optimising remember that the required througput here is much lower than the throughput of the server
//consider running the processing in a single thread

class office
{
public:
  //office(boost::asio::io_service& io_service,const boost::asio::ip::tcp::endpoint& endpoint,options& opts,server& srv) :
  office(options& opts,server& srv) :
    endpoint_(boost::asio::ip::tcp::v4(),opts.offi),
    io_service_(),
    work_(io_service_),
    acceptor_(io_service_,endpoint_),
    srv_(srv),
    opts_(opts),
    fd(0),
    message_sent(0),
    next_io_service_(0)
  { svid=opts_.svid;
    std::cerr<<"OFFICE ("<<svid<<") open\n";
    // prapare local register [this could be in RAM later or be on a RAM disk]
    mkdir("ofi",0755);
    char filename[64];
    sprintf(filename,"ofi/%04X.dat",svid);
    fd=open(filename,O_RDWR|O_CREAT|O_TRUNC,0644); // truncate to force load from main repository
    if(fd<0){
      std::cerr<<"ERROR, failed to open office register\n";}
  }

  ~office()
  { if(fd){
      close(fd);}
    std::cerr<<"Office down\n";
  }
  void iorun_client(int i)
  { while(1){
      try{
        std::cerr << "Client["<<i<<"].Run starting\n";
        io_services_[i]->run();
        std::cerr << "Client["<<i<<"].Run finished\n";
        return;} //Now we know the server is down.
      catch (std::exception& e){
        std::cerr << "Client["<<i<<"].Run error: " << e.what() << "\n";}}
  }
  void iorun()
  { while(1){
      try{
        std::cerr << "Office.Run starting\n";
        io_service_.run();
        std::cerr << "Office.Run finished\n";
        return;} //Now we know the server is down.
      catch (std::exception& e){
        std::cerr << "Office.Run error: " << e.what() << "\n";}}
  }
  void stop() // send and empty txs queue
  { run=false;
    io_service_.stop();
    ioth_->join();
    for(int i=0;i<CLIENT_POOL;i++){
      io_services_[i]->stop();}
    threadpool.interrupt_all();
    threadpool.join_all();
    clock_thread->interrupt();
    clock_thread->join();
    //submit message
  }

  void start(uint32_t myusers)
  { memcpy(pkey,srv_.pkey,32);
    users=myusers; //FIXME !!! this maybe incorrect !!!
    //msid=srv_.msid_;
    deposit.resize(users); //a buffer to enable easy resizing of the account vector
    //copy bank file
    char filename[64];
    sprintf(filename,"usr/%04X.dat",svid);
    int gd=open(filename,O_RDONLY);
    if(gd<0){
      fprintf(stderr,"ERROR, failed to open %s, fatal\n",filename);
      exit(-1);}
    for(uint32_t user=0;user<users;user++){
      user_t u;
      int len=read(gd,&u,sizeof(user_t));
      if(len!=sizeof(user_t)){
        fprintf(stderr,"ERROR, failed to read %s after %d (%08X) users, fatal\n",filename,user,user);
        exit(-1);}
      deposit[user]=srv_.dividend(u); //use deposit to remember data for log entries
      write(fd,&u,sizeof(user_t));}
    close(gd);

    //init io_service_pool
    run=true; //not used yet
    div_ready=0;
    block_ready=0;
    for(int i=0;i<CLIENT_POOL;i++){
      io_services_[i]=boost::make_shared<boost::asio::io_service>();
      io_works_[i]=boost::make_shared<boost::asio::io_service::work>(*io_services_[i]);
      threadpool.create_thread(boost::bind(&office::iorun_client,this,i));}
    ioth_ = new boost::thread(boost::bind(&office::iorun, this));
    clock_thread = new boost::thread(boost::bind(&office::clock, this));
  }

  void update_div(uint32_t now,uint32_t newdiv)
  { char filename[64];
    sprintf(filename,"ofi/%04X_%08X.div",svid,now); // log dividends, save in user logs later
    int dd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(dd<0){
      fprintf(stderr,"ERROR, failed to open %s, fatal\n",filename);
      exit(-1);}
    sprintf(filename,"usr/%04X.dat",svid);
    int gd=open(filename,O_RDONLY);
    if(gd<0){
      fprintf(stderr,"ERROR, failed to open %s, fatal\n",filename);
      exit(-1);}
    for(uint32_t user=0;user<users;user++){
      user_t u;
      int len=read(gd,&u,sizeof(user_t));
      if(len!=sizeof(user_t)){
        fprintf(stderr,"ERROR, failed to read global user %08X, fatal\n",user);
        exit(-1);}
      int64_t div=(u.weight>>16)*newdiv-TXS_USR_FEE;
      if(div<-u.weight){
        div=-u.weight;}
      write(dd,&div,sizeof(uint64_t));}
    close(gd);
    close(dd);
  }

  void process_div(uint32_t now)
  { int dd=-1;
    char filename[64];
    log_t alog;
    if(now){ //if now==0: firs dididend update load from deposit[]
      sprintf(filename,"ofi/%04X_%08X.div",svid,now); // log dividends, save in user logs later
      dd=open(filename,O_RDONLY);
      if(dd<0){
        fprintf(stderr,"ERROR, failed to open %s, fatal\n",filename);
        exit(-1);}
      alog.type=TXSTYPE_CON;}
    else{
      alog.type=TXSTYPE_STP;}
    for(uint32_t user=0;user<users;user++){
      int64_t div;
      if(now){
        if(read(dd,&div,sizeof(uint64_t))!=sizeof(uint64_t)){
          break;}
        add_deposit(user,div);}
      else{ // no lock needed
        if(deposit[user]==(int64_t)0x8FFFFFFFFFFFFFFF){
          deposit[user]=0;}
        div=deposit[user];
        deposit[user]=0;}
      alog.time=now;
      alog.node=svid;
      alog.user=user;
      alog.umid=now;
      alog.nmid=0;
      alog.mpos=0;
      alog.weight=div;
      bzero(alog.info,32);
      srv_.put_log(svid,user,alog);} //assume, no lock needed
    if(now){
      close(dd);
      unlink(filename);}
  }

  void update_block(uint32_t period_start,uint32_t now,message_queue& commit_msgs,uint32_t newdiv)
  { for(auto mi=commit_msgs.begin();mi!=commit_msgs.end();mi++){
      uint64_t svms=(uint64_t)((*mi)->svid)<<32|(*mi)->msid;
      mque.push_back(svms);}
    if(period_start==now){
      update_div(now,newdiv);
      div_ready=now;}
    block_ready=now;
  }

  void process_log(uint32_t now)
  { uint32_t lpos=0;
    const uint32_t maxl=0xffff;
    std::map<uint64_t,log_t> log;
    for(auto mi=mque.begin();mi!=mque.end();mi++){
      uint64_t svms=(*mi);
      uint32_t svid=svms>>32;
      uint32_t msid=svms&0xFFFFFFFF;
      char filename[64];
      sprintf(filename,"blk/%03X/%05X/log/%04X_%08X.log",now>>20,now&0xFFFFF,svid,msid);
      int fd=open(filename,O_RDONLY);
      if(fd<0){
        std::cerr<<"ERROR, failed to open log file "<<filename<<"\n";
        continue;}
      while(1){
        uint32_t user;
        log_t ulog;
        if(read(fd,&user,sizeof(uint32_t))<=0){
          break;}
        if(read(fd,&ulog,sizeof(log_t))<=0){
          break;}
        uint64_t lkey=(uint64_t)(user)<<32|lpos++;
        log[lkey]=ulog;
        if(lpos>=maxl){
          srv_.put_log(svid,log);
          log.clear();
          lpos=0;}}
      close(fd);}
    if(lpos){
      srv_.put_log(svid,log);}
    mque.clear();
  }

  void clock()
  { process_div(0);
    start_accept();
    while(run){
      uint32_t now=time(NULL);
//FIXME, do not submit messages in vulnerable time (from blockend-margin to new block confirmation)
      //TODO, clear hanging clients
      boost::this_thread::sleep(boost::posix_time::seconds(2));
      if(block_ready){
        fprintf(stderr,"OFFICE, process last block %08X\n",block_ready-BLOCKSEC);
        if(div_ready){
          fprintf(stderr,"OFFICE, process dividends @ %08X\n",div_ready);
          process_div(div_ready);} // process DIVIDENDs
        process_log(block_ready-BLOCKSEC); // process logs
        process_gup(block_ready-BLOCKSEC); // process GET transactions
        process_dep(block_ready-BLOCKSEC); // process PUT remote deposits
        if(block_ready!=srv_.srvs_now()){
          fprintf(stderr,"ERROR, failed to finish local update on time, fatal\n");
          exit(-1);}
        block_ready=0;
        div_ready=0;}
      if(message.empty()){
        message_.lock();
        srv_.break_silence(now,message);
        message_.unlock();
        continue;}
      if(message.length()<MESSAGE_LEN_OK && message_sent+MESSAGE_WAIT>now){
	std::cerr<<"WARNING, waiting for more messages\n";
        continue;}
      //if(!srv_.accept_message(msid)){
      if(!srv_.accept_message()){
	std::cerr<<"WARNING, server not ready for a new message ("<<srv_.msid_<<")\n";
        continue;}
      message_.lock();
      std::cerr<<"SENDING new message (old msid:"<<srv_.msid_<<")\n";
      srv_.write_message(message);
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

  void process_gup(uint32_t now)
  { user_t u;
    while(!gup.empty()){
      gup_t& cgup=gup.top();
      assert(cgup.auser<users);
      file_.lock();
      lseek(fd,cgup.auser*sizeof(user_t),SEEK_SET);
      read(fd,&u,sizeof(user_t));
      u.node=cgup.node;
      u.user=cgup.user;
      u.time=cgup.time;
      u.rpath=now;
      u.weight+=deposit[cgup.auser]-cgup.delta;
      deposit[cgup.auser]=0;
      lseek(fd,-sizeof(user_t),SEEK_CUR);
      write(fd,&u,sizeof(user_t));
      file_.unlock();
      gup.pop();}
  }

  void process_dep(uint32_t now)
  { user_t u;
    while(!rdep.empty()){
      dep_t& dep=rdep.top();
      assert(dep.auser<users);
      file_.lock();
      lseek(fd,dep.auser*sizeof(user_t),SEEK_SET);
      read(fd,&u,sizeof(user_t));
      u.rpath=now;
      u.weight+=deposit[dep.auser]+dep.weight;
      deposit[dep.auser]=0;
      lseek(fd,-sizeof(user_t),SEEK_CUR);
      write(fd,&u,sizeof(user_t));
      file_.unlock();
      rdep.pop();}
  }

  bool get_user_global(user_t& u,uint16_t cbank,uint32_t cuser)
  { u.msid=0;
    srv_.last_srvs_.get_user(u,cbank,cuser);
    if(!u.msid){
      return(false);}
    srv_.dividend(u); //add missing dividend
    return(true);
  }

  bool get_user(user_t& u,uint16_t cbank,uint32_t cuser)
  { u.msid=0;
    if(cbank!=svid){
      return(get_user_global(u,cbank,cuser));}
    if(cuser>=users){
      return(false);}
    file_.lock();
    lseek(fd,cuser*sizeof(user_t),SEEK_SET);
    read(fd,&u,sizeof(user_t));
    if(!u.msid){
      file_.unlock();
      return(false);}
    if(deposit[cuser]){
      u.weight+=deposit[cuser];
      deposit[cuser]=0;
      assert(u.weight>=0);
      lseek(fd,-sizeof(user_t),SEEK_CUR);
      write(fd,&u,sizeof(user_t));}
    file_.unlock();
    return(true);
  }

  void add_remote_user(uint16_t bbank,uint32_t buser,uint8_t* pkey) //create account on remote request
  { if(!try_account((hash_s*)pkey)){
      std::cerr<<"ERROR: failed to open account (pkey known)\n";
      return;}
    uint32_t ltime=time(NULL);
    uint32_t luser=add_user(bbank,pkey,ltime,buser);
    if(luser){
      uint32_t msid;
      uint32_t mpos;
      usertxs_ptr txs(new usertxs(TXSTYPE_UOK,svid,luser,0,ltime,bbank,buser,0,0,(const char*)pkey));
      add_msg(txs->data,txs->size,0,msid,mpos); //FIXME, fee ???
      add_key((hash_s*)pkey,luser);} //blacklist
  }

  uint32_t add_user(uint16_t abank,uint8_t* pk,uint32_t when,uint32_t auser) // will create new account or overwrite old one
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
//FIXME, do we need LOCK_TIME check ?
      if(now>nu.lpath+LOCK_TIME && nu.weight<=0){ //-TIME_FEE(now,nu.lpath) // try changing this account
//FIXME, do not change accounts that are open for too short
      //if(nu.status & USER_CLOSED){ // try changing this account
        file_.lock();
        lseek(fd,nuser*sizeof(user_t),SEEK_SET);
        read(fd,&nu,sizeof(user_t));
//FIXME !!!
//FIXME, network conflict ... somebody can wire funds to this account while the account is beeing overwritten
//FIXME !!! (maybe create a slow 'account close' transaction)
        if(nu.weight<=0){ // commit changing this account
//FIXME, do not change accounts that are open for too short
          //std::cerr<<"WARNING, overwriting account "<<nuser<<"\n";
          fprintf(stderr,"WARNING, overwriting empty account %08X [weight:%016lX]\n",
            nuser,nu.weight);
          //FIXME !!!  wrong time !!! must use time from txs
          srv_.last_srvs_.init_user(nu,svid,nuser,(abank==svid?MIN_MASS:0),pk,when,abank,auser);
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
    srv_.last_srvs_.init_user(nu,svid,nuser,(abank==svid?MIN_MASS:0),pk,when,abank,auser);
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
//fprintf(stderr,"\n\nSET USER WRITE\n\n");
    lseek(fd,user*sizeof(user_t),SEEK_SET);
    write(fd,&u,sizeof(user_t));
    file_.unlock();
  }

  void add_remote_deposit(uint32_t buser,int64_t tmass)
  { dep_t dep={buser,tmass};
    rdep.push(dep);
//fprintf(stderr,"\n\nADDING REMOTE DEPOSIT\n\n");
  }

  void add_deposit(uint32_t buser,int64_t tmass)
  { file_.lock();
    deposit[buser]+=tmass;
    file_.unlock();
//fprintf(stderr,"\n\nADDING local DEPOSIT\n\n");
  }

  void add_deposit(usertxs& utxs)
  { file_.lock();
    deposit[utxs.buser]+=utxs.tmass;
    file_.unlock();
//fprintf(stderr,"\n\nADDING local DEPOSIT\n\n");
  }

  bool try_account(hash_s* key)
  { account_.lock();
    if(accounts_.size()>MAX_ACCOUNT){
      for(auto it=accounts_.begin();it!=accounts_.end();){
        auto jt=it++;
        user_t u;
        get_user(u,svid,jt->second); //watch out for deadlocks
        //if(u.weight>=MIN_MASS){
        if(u.weight>0){
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

  void add_key(hash_s* key,uint32_t user)
  { account_.lock();
    accounts_[*key]=user;
    account_.unlock();
  }

  void add_msg(uint8_t* msg,int len, int64_t fee,uint32_t& msid,uint32_t& mpos)
  { if(!run){ return;}
    message_.lock();
    msid=srv_.msid_+1;
    mpos=message.length()+message::data_offset;
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
  void leave(client_ptr c); // main.cpp

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

  uint32_t last_path()
  { return(srv_.last_srvs_.now);
  }

  void put_log(uint16_t svid,uint32_t user,log_t& log)
  { srv_.put_log(svid,user,log);
  }

  void put_log(uint32_t now,uint16_t svid,uint32_t msid,std::map<uint64_t,log_t>& log)
  { srv_.put_log(now,svid,msid,log);
  }

  void put_log(uint16_t svid,std::map<uint64_t,log_t>& log)
  { srv_.put_log(svid,log);
  }

  uint16_t svid;
  hash_t pkey; // local copy for managing updates
  std::stack<gup_t> gup; // GET results
private:
  bool run;
  uint32_t div_ready;
  uint32_t block_ready;
  uint32_t users; //number of users of the bank
  std::string message;
  std::vector<int64_t> deposit; //resizing will require a stop of processing
  boost::asio::ip::tcp::endpoint endpoint_;
  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  boost::asio::ip::tcp::acceptor acceptor_;
  boost::thread* ioth_;
  server& srv_;
  options& opts_;
  std::set<client_ptr> clients_;
  int fd; // user file descriptor
  //uint32_t msid;
   int64_t mfee; // let's see if we need this
  uint32_t message_sent;
  boost::thread* clock_thread;
  std::map<hash_s,uint32_t,hash_cmp> accounts_; // list of candidates

  //io_service_pool
  typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
  typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;
  io_service_ptr io_services_[CLIENT_POOL];
  work_ptr io_works_[CLIENT_POOL];
  int next_io_service_;
  boost::thread_group threadpool;
  std::stack<dep_t> rdep; // users with remote deposits
  std::deque<uint64_t> mque; // list of message to process log

  boost::mutex client_;
  boost::mutex file_;
  boost::mutex account_;
  boost::mutex message_;
  boost::mutex users_[0x100];
};

#endif // OFFICE_HPP
