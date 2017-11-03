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
    offifd_(-1),
    message_sent(0),
    message_tnum(0),
    next_io_service_(0),
    ioth_(NULL),
    clock_thread(NULL)
  { svid=opts_.svid;
    try{
      DLOG("OFFICE %04X open\n",svid);
      if(svid){
        // prapare local register [this could be in RAM later or be on a RAM disk]
        mkdir("ofi",0755);
        //char filename[64];
        sprintf(ofifilename,"ofi/%04X.dat",svid);
        offifd_=open(ofifilename,O_RDWR|O_CREAT|O_TRUNC,0644); // truncate to force load from main repository
        if(offifd_<0){
          ELOG("ERROR, failed to open office register\n");}
        mklogdir(opts_.svid);
        mklogfile(opts_.svid,0);}}
    catch (std::exception& e){
      ELOG("Office.Open error: %s\n",e.what());}
  }

  ~office()
  { if(offifd_>=0){
      close(offifd_);}
    ELOG("Office down\n");
  }
  void iorun_client(int i)
  { while(1){
      try{
        DLOG("Client[%d].Run starting\n",i);
        io_services_[i]->run();
        DLOG("Client[%d].Run finished\n",i);
        return;} //Now we know the server is down.
      catch (std::exception& e){
        DLOG("Client[%d].Run error: %s\n",i,e.what());}}
  }
  void iorun()
  { while(1){
      try{
        DLOG("Office.Run starting\n");
        io_service_.run();
        DLOG("Office.Run finished\n");
        return;} //Now we know the server is down.
      catch (std::exception& e){
        DLOG("Office.Run error: %s\n",e.what());}}
  }
  void stop() // send and empty txs queue
  { run=false;
    io_service_.stop();
    if(ioth_!=NULL){
      ioth_->join();
      for(int i=0;i<CLIENT_POOL;i++){
        io_services_[i]->stop();}}
    threadpool.interrupt_all();
    threadpool.join_all();
    if(clock_thread!=NULL){
      clock_thread->interrupt();
      clock_thread->join();}
    DLOG("Office shutdown completed\n");
  }

  void init(uint32_t myusers)
  { memcpy(pkey,srv_.pkey,32);
    users=myusers; //FIXME !!! this maybe incorrect !!!
    if(!myusers){
      assert(!svid);
      return;}
    //msid=srv_.msid_;
  //deposit.resize(users); //a buffer to enable easy resizing of the account vector
  //ustatus.resize(users); //a buffer to enable easy resizing of the account vector
    //copy bank file
    char filename[64];
    sprintf(filename,"usr/%04X.dat",svid);
    int gd=open(filename,O_RDONLY);
    if(gd<0){
      DLOG("ERROR, failed to open %s in init, fatal\n",filename);
      exit(-1);}
    log_.lock();
    log_t alog;
    alog.type=TXSTYPE_NON|0x8000; //incoming
    alog.time=time(NULL);
    alog.nmid=srv_.start_msid;
    alog.mpos=srv_.start_path;
    for(uint32_t user=0;user<users;user++){
      user_t u;
      int len=read(gd,&u,sizeof(user_t));
      if(len!=sizeof(user_t)){
        DLOG("ERROR, failed to read %s after %d (%08X) users, fatal\n",filename,user,user);
        exit(-1);}
      // record initial user status
      memcpy(alog.info+ 0,&u.weight,8);
      memcpy(alog.info+ 8,u.hash,8);
      memcpy(alog.info+16,&u.lpath,4);
      memcpy(alog.info+20,&u.rpath,4);
      memcpy(alog.info+24,u.pkey,6);
      memcpy(alog.info+30,&u.stat,2);
      int64_t div=srv_.dividend(u); //use deposit to remember data for log entries
      if(div==(int64_t)0x8FFFFFFFFFFFFFFF){
        div=0;}
      alog.umid=u.msid;
      alog.node=u.node;
      alog.user=u.user;
      alog.weight=div;
      if(u.stat&USER_STAT_DELETED){
        deleted_users.push_back(user);}
      write(offifd_,&u,sizeof(user_t)); // write all ok
      // check last log status
      //log_t llog;
      //srv_.get_lastlog(svid,user,llog);
      put_log(user,alog);}
    close(gd);
    log_.unlock();
  }

  void start()
  { //init io_service_pool
    RETURN_ON_SHUTDOWN();
    run=true; //not used yet
    div_ready=0;
    block_ready=0;
    try{
      for(int i=0;i<CLIENT_POOL;i++){
        io_services_[i]=boost::make_shared<boost::asio::io_service>();
        io_works_[i]=boost::make_shared<boost::asio::io_service::work>(*io_services_[i]);
        threadpool.create_thread(boost::bind(&office::iorun_client,this,i));}
      ioth_ = new boost::thread(boost::bind(&office::iorun, this));
      clock_thread = new boost::thread(boost::bind(&office::clock, this));}
    catch (std::exception& e){
      DLOG("Office.Start error: %s\n",e.what());}
  }

  void update_div(uint32_t now,uint32_t newdiv)
  { if(!svid){
      return;}
    char filename[64];
    sprintf(filename,"ofi/%04X_%08X.div",svid,now); // log dividends, save in user logs later
    int dd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(dd<0){
      DLOG("ERROR, failed to open %s, fatal\n",filename);
      exit(-1);}
    sprintf(filename,"usr/%04X.dat",svid);
    int gd=open(filename,O_RDONLY);
    if(gd<0){
      DLOG("ERROR, failed to open %s, fatal\n",filename);
      exit(-1);}
    sprintf(filename,"ofi/%04X.dat",svid);
    int ld=open(filename,O_RDONLY);
    if(ld<0){
      DLOG("ERROR, failed to open %s, fatal\n",filename);
      exit(-1);}
    for(uint32_t user=0;user<users;user++){
      user_t u;
      int len=read(gd,&u,sizeof(user_t));
      if(len!=sizeof(user_t)){
        lseek(ld,user*sizeof(user_t),SEEK_SET);
        len=read(ld,&u,sizeof(user_t));
        if(len!=sizeof(user_t)){
          DLOG("ERROR, failed to read local user %08X, fatal\n",user);
          exit(-1);}
        if(u.rpath>=now){ // should be a rare case, but maybe possible
          int64_t div=0;
          write(dd,&div,sizeof(uint64_t));
          continue;}
        u.weight=USER_MIN_MASS;}
      int64_t div=(u.weight>>16)*newdiv-TXS_DIV_FEE;
      if(div<-u.weight){
        div=-u.weight;}
      write(dd,&div,sizeof(uint64_t));}
    close(ld);
    close(gd);
    close(dd);
  }

  void process_div(uint32_t path)
  { if(!svid){
      return;}
    int dd=-1;
    char filename[64];
    sprintf(filename,"ofi/%04X_%08X.div",svid,path); // log dividends, save in user logs later
    dd=open(filename,O_RDONLY);
    if(dd<0){
      DLOG("ERROR, failed to open %s, fatal\n",filename);
      exit(-1);}
    // read 2 times to prevent double lock
    for(uint32_t user=0;user<users;user++){
      int64_t div;
      if(read(dd,&div,sizeof(uint64_t))!=sizeof(uint64_t)){
        break;}
      add_deposit(user,div);}//LOCK: file_
    lseek(dd,0,SEEK_SET);
    log_.lock();
    log_t alog;
    alog.time=time(NULL);
    alog.node=0;
    alog.user=0;
    alog.umid=0;
    alog.nmid=srv_.msid_;
    alog.mpos=path; 
    alog.type=TXSTYPE_DIV|0x8000; //incoming
    bzero(alog.info,32);
    for(uint32_t user=0;user<users;user++){
      int64_t div;
      if(read(dd,&div,sizeof(uint64_t))!=sizeof(uint64_t)){
        break;}
      alog.weight=div;
      put_log(user,alog);}
    close(dd);
    log_.unlock();
    unlink(filename);
  }

  void update_block(uint32_t period_start,uint32_t now,message_map& commit_msgs,uint32_t newdiv)
  { for(auto mi=commit_msgs.begin();mi!=commit_msgs.end();mi++){
      uint64_t svms=((uint64_t)(mi->second->svid)<<32)|mi->second->msid;
      mque.push_back(svms);}
    //DLOG("UPDATE LOG processed queue\n");
    if(period_start==now){
      assert(now);
      update_div(now,newdiv);
      //DLOG("UPDATE LOG processed div\n");
      div_ready=now;}
    block_ready=now;
    //DLOG("UPDATE LOG return\n");
  }

  void process_log(uint32_t now) //FIXME, check here if logs are not duplicated !!! maybe record stored info
  { if(!svid){
      return;}
    uint32_t lpos=0;
    const uint32_t maxl=0xffff;
    std::map<uint64_t,log_t> log;
    mque.push_back(0); //FIXME, remove later
    //uint64_t sv00=((uint64_t)opts_.svid)<<32;
    //mque.push_back(sv00); //add block message
    char filename[64];
    sprintf(filename,"blk/%03X/%05X/log/time.bin",now>>20,now&0xFFFFF);
    int fd=open(filename,O_RDWR|O_CREAT,0644);
    if(fd<0){
      DLOG("ERROR, failed to open log time file %s, fatal\n",filename);
      exit(-1);}
    struct stat sb;
    fstat(fd,&sb);
    log_.lock();
    uint32_t ntime=time(NULL);
    if(sb.st_size){ // file is ok
//FIXME, !!! master log hygiene
      DLOG("\nERROR, log time file %s not empty, inform all users about change\n\n",filename);
      uint32_t otime;
      read(fd,&otime,sizeof(uint32_t));
      log_t alog;
      bzero(&alog,sizeof(log_t));
      alog.type=TXSTYPE_NON|0x4000;
      alog.time=ntime;
      alog.mpos=now;
      alog.nmid=otime;
      for(uint32_t user=0;user<users;user++){
        put_log(user,alog);}
      lseek(fd,0,SEEK_SET);}
    write(fd,&ntime,sizeof(uint32_t));
    close(fd);
    for(auto mi=mque.begin();mi!=mque.end();mi++){
      uint64_t svms=(*mi);
      uint32_t bank=svms>>32;
      uint32_t msid=svms&0xFFFFFFFF;
      sprintf(filename,"blk/%03X/%05X/log/%04X_%08X.log",now>>20,now&0xFFFFF,bank,msid);
      int fd=open(filename,O_RDONLY);
      if(fd<0){
        DLOG("OFFICE, failed to open log file %s\n",filename);
        continue;}
      while(1){
        uint32_t user;
        log_t ulog;
        if(read(fd,&user,sizeof(uint32_t))<=0){
          break;}
        if(read(fd,&ulog,sizeof(log_t))<=0){
          break;}
        uint64_t lkey=(uint64_t)(user)<<32|lpos++;
        log[lkey]=ulog;}
      close(fd);
      if(lpos>=maxl){ //TODO, maybe record what was processed !!!
        put_log(log,ntime);
        log.clear();
        lpos=0;}}
    if(lpos){
      put_log(log,ntime);}
    log_.unlock();
    mque.clear();
  }

  void clock()
  { //while(run){
    //  if(srv_.msid_>=srv_.start_msid){
    //    break;}
    //  DLOG("OFFICE, wait for server to read last message %08X\n",srv_.start_msid);
    //  boost::this_thread::sleep(boost::posix_time::seconds(2));}
    //if(run){
    //  get_msg(srv_.msid_+1);
    //  start_accept();}
    start_accept();
    while(run){
      uint32_t now=time(NULL);
//FIXME, do not submit messages in vulnerable time (from blockend-margin to new block confirmation)
      //TODO, clear hanging clients
      boost::this_thread::sleep(boost::posix_time::seconds(2));
      if(block_ready){
        DLOG("OFFICE, process last block %08X\n",block_ready-BLOCKSEC);
        if(div_ready){
          DLOG("OFFICE, process dividends @ %08X\n",div_ready);
          process_div(div_ready);} // process DIVIDENDs
        process_log(block_ready-BLOCKSEC); // process logs
        process_gup(block_ready-BLOCKSEC); // process GET transactions
        process_dep(block_ready-BLOCKSEC); // process PUT remote deposits
        if(block_ready!=srv_.srvs_now()){
          DLOG("ERROR, failed to finish local update on time, fatal\n");
          exit(-1);}
        block_ready=0;
        div_ready=0;}
      if(message.empty()){
#ifdef DEBUG
        file_.lock();
        srv_.break_silence(now,message,message_tnum);
        file_.unlock();
#endif
        continue;}
      assert(svid);
      if(message.length()<MESSAGE_LEN_OK && message_tnum<MESSAGE_TNUM_OK && message_sent+MESSAGE_WAIT>now){
	DLOG("WARNING, waiting for more messages\n");
        continue;}
      if(!srv_.accept_message()){
	DLOG("WARNING, server not ready for a new message (%d)\n",srv_.msid_);
        continue;}
      file_.lock();
      uint32_t newmsid=srv_.msid_+1;
      DLOG("SENDING message %08X\n",newmsid);
#ifdef DOUBLE_SPEND
      if(opts_.svid == 4 && !(newmsid%8) && !(rand()%2)){
        DLOG("\n\nNICE sending double spend message %08X\n\n\n",newmsid);
        srv_.write_dblspend(message);}
      else
#endif
      if(!srv_.write_message(message)){
        file_.unlock();
        DLOG("ERROR sending message %08X\n",newmsid);
        continue;}
      message.clear();
      message_tnum=0;
      file_.unlock();
      del_msg(newmsid);
      message_sent=now;}
  }

  /*bool check_account(uint16_t cbank,uint32_t cuser)
  { if(cbank==svid && cuser<users){
      return(true);}
    if(srv_.last_srvs_.nodes.size()<=cbank){
      return(false);}
    if(srv_.last_srvs_.nodes[cbank].users<=cuser){
      return(false);}
    return(true);
  }*/

  void process_gup(uint32_t now)
  { if(!svid){
      return;}
    user_t u;
    while(!gup.empty()){
      gup_t& cgup=gup.top();
      assert(cgup.auser<users);
      file_.lock();
      lseek(offifd_,cgup.auser*sizeof(user_t),SEEK_SET);
      read(offifd_,&u,sizeof(user_t));
      u.node=cgup.node;
      u.user=cgup.user;
      u.time=cgup.time;
      u.rpath=now;
      u.weight-=cgup.delta;
      //u.weight+=deposit[cgup.auser]-cgup.delta;
      //deposit[cgup.auser]=0;
      lseek(offifd_,-sizeof(user_t),SEEK_CUR);
      write(offifd_,&u,sizeof(user_t)); // write node,user,time,rpath,weight
      file_.unlock();
      gup.pop();}
  }

  void process_dep(uint32_t now)
  { if(!svid){
      return;}
    user_t u;
    while(!rdep.empty()){
      dep_t& dep=rdep.top();
      assert(dep.auser<users);
      file_.lock();
      lseek(offifd_,dep.auser*sizeof(user_t),SEEK_SET);
      read(offifd_,&u,sizeof(user_t));
      u.rpath=now;
      u.weight+=dep.weight;
      //u.weight+=deposit[dep.auser]+dep.weight;
      //deposit[dep.auser]=0;
      lseek(offifd_,-sizeof(user_t),SEEK_CUR);
      write(offifd_,&u,sizeof(user_t)); // write rpath,weight
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
    if(!svid){
      bzero((void*)&u,sizeof(user_t));
      return(true);}
    if(cuser>=users){
      return(false);}
    //file_.lock(); //FIXME, could use read only access without lock
    int fd=open(ofifilename,O_RDONLY);
    if(fd<0){ ELOG("ERROR, failed to open account file %s\n",ofifilename);
      return(false);}
    lseek(fd,cuser*sizeof(user_t),SEEK_SET);
    read(fd,&u,sizeof(user_t));
    close(fd);
    //file_.unlock();
    if(!u.msid){
      return(false);}
    //if(deposit[cuser] || ustatus[cuser]!=u.status)
    //if(deposit[cuser]){
    //  u.weight+=deposit[cuser];
    //  deposit[cuser]=0;
    //  //u.status=ustatus[cuser];
    //  assert(u.weight>=0);
    //  lseek(offifd_,-sizeof(user_t),SEEK_CUR);
    //  write(offifd_,&u,sizeof(user_t));} // write weight
    return(true);
  }

  void add_remote_user(uint16_t bbank,uint32_t buser,uint8_t* pkey) //create account on remote request
  { assert(svid);
    if(!try_account((hash_s*)pkey)){
      ELOG("ERROR: failed to open account (pkey known)\n");
      return;}
    uint32_t ltime=time(NULL);
    uint32_t luser=add_user(bbank,pkey,ltime,buser);
    if(luser){
      uint32_t msid;
      uint32_t mpos;
      usertxs_ptr txs(new usertxs(TXSTYPE_UOK,svid,luser,0,ltime,bbank,buser,0,NULL,(const char*)pkey));
      add_msg(txs->data,*txs,msid,mpos);
      if(msid){
        add_key((hash_s*)pkey,luser);}} //blacklist
  }

  uint32_t add_user(uint16_t abank,uint8_t* pk,uint32_t when,uint32_t auser) // will create new account or overwrite old one
  { assert(svid);
    uint32_t nuser;
    user_t nu;
    file_.lock();
    while(!deleted_users.empty()){
      nuser=deleted_users.front();
      deleted_users.pop_front();
      lseek(offifd_,nuser*sizeof(user_t),SEEK_SET);
      read(offifd_,&nu,sizeof(user_t));
      if((nu.weight<=0) && (nu.stat&USER_STAT_DELETED)){
        DLOG("WARNING, overwriting empty account %08X [weight:%016lX]\n",nuser,nu.weight);
        //FIXME !!!  wrong time !!! must use time from txs
        srv_.last_srvs_.init_user(nu,svid,nuser,(abank==svid?USER_MIN_MASS:0),pk,when,abank,auser);
        lseek(offifd_,-sizeof(user_t),SEEK_CUR);
        write(offifd_,&nu,sizeof(user_t)); // write all ok
        file_.unlock();
        return(nuser);}}
    file_.unlock();
    if(users>=MAX_USERS){
      return(0);}
    // no old account found, creating new account
    //FIXME !!!  wrong time !!! must use time from txs
    nuser=users++;
    mklogfile(svid,nuser);
    srv_.last_srvs_.init_user(nu,svid,nuser,(abank==svid?USER_MIN_MASS:0),pk,when,abank,auser);
    ELOG("CREATING new account %d\n",nuser);
    file_.lock();
  //deposit.push_back(0);
  //ustatus.push_back(0);
    lseek(offifd_,nuser*sizeof(user_t),SEEK_SET);
    write(offifd_,&nu,sizeof(user_t)); // write all ok
    file_.unlock();
    return(nuser);
  }

  void set_user(uint32_t user,user_t& nu, int64_t deduct)
  { assert(user<users); // is this safe ???
    user_t ou;
    file_.lock();
    lseek(offifd_,user*sizeof(user_t),SEEK_SET);
    read(offifd_,&ou,sizeof(user_t));
    ou.weight-=deduct;
    //ou.weight+=deposit[user]-deduct;
    //deposit[user]=0;
    ou.msid=nu.msid;
    ou.time=nu.time;
    ou.node=nu.node;
    ou.user=nu.user;
    ou.lpath=nu.lpath;
    memcpy(ou.hash,nu.hash,32);
    memcpy(ou.pkey,nu.pkey,32);
  //u.status=ustatus[user];
    //assert(u.weight>=0);
    lseek(offifd_,-sizeof(user_t),SEEK_CUR);
    write(offifd_,&ou,sizeof(user_t)); // fix this !!!
    file_.unlock();
  }

  void delete_user(uint32_t user)
  { assert(user<users); // is this safe ???
    file_.lock();
    user_t u;
    lseek(offifd_,user*sizeof(user_t),SEEK_SET);
    read(offifd_,&u,sizeof(user_t));
    u.stat|=USER_STAT_DELETED;
    if(u.weight>=0){
      DLOG("WARNNG: network deleted active user %08X\n",user);}
    lseek(offifd_,-sizeof(user_t),SEEK_CUR);
    write(offifd_,&u,sizeof(user_t)); // write all ok
    deleted_users.push_back(user);
    file_.unlock();
  }

  void add_remote_deposit(uint32_t buser,int64_t tmass)
  { assert(svid);
    dep_t dep={buser,tmass};
    rdep.push(dep);
  }

  void add_deposit(uint32_t buser,int64_t tmass)
  { assert(svid);
    const int dep_off=(char*)&user_tmp.weight-(char*)&user_tmp;
    int64_t w;
    file_.lock();
    lseek(offifd_,buser*sizeof(user_t)+dep_off,SEEK_SET);
    read(offifd_,&w,sizeof(w));
    w+=tmass;
    lseek(offifd_,-sizeof(w),SEEK_CUR);
    write(offifd_,&w,sizeof(w));
    //deposit[buser]+=tmass;
    file_.unlock();
  }

  void add_deposit(usertxs& utxs)
  { assert(svid);
    const int dep_off=(char*)&user_tmp.weight-(char*)&user_tmp;
    int64_t w;
    file_.lock();
    lseek(offifd_,utxs.buser*sizeof(user_t)+dep_off,SEEK_SET);
    read(offifd_,&w,sizeof(w));
    w+=utxs.tmass;
    lseek(offifd_,-sizeof(w),SEEK_CUR);
    write(offifd_,&w,sizeof(w));
    //deposit[utxs.buser]+=utxs.tmass;
    file_.unlock();
  }

  bool try_account(hash_s* key)
  { assert(svid);
    account_.lock();
    if(accounts_.find(*key)!=accounts_.end()){
      account_.unlock();
      return(false);}
    if(accounts_.size()>MAX_USER_QUEUE){
      std::map<hash_s,uint32_t,hash_cmp> acs = accounts_;
      account_.unlock();
      std::vector<hash_s> del;
      for(auto it=acs.begin();it!=acs.end();it++){
        user_t u;
        get_user(u,svid,it->second);
        if(u.weight>0){
          del.push_back(it->first);}}
      account_.lock();
      for(auto jt : del){
        accounts_.erase(jt);}
      if(accounts_.size()>MAX_USER_QUEUE){
        account_.unlock();
        return(false);}}
    account_.unlock();
    return(true);
  }

  void add_key(hash_s* key,uint32_t user)
  { assert(svid);
    account_.lock();
    accounts_[*key]=user;
    account_.unlock();
  }

  bool get_msg(uint32_t msid,std::string& line)
  { char filename[64];
    sprintf(filename,"ofi/msg_%08X.msd",msid);
    int md=open(filename,O_RDONLY);
    if(md<0){
      return(false);} // :-( maybe we should throw here something
    struct stat sb;
    fstat(md,&sb);
    if(!sb.st_size){
      close(md);
      return(false);}
    char msg[sb.st_size];
    read(md,msg,sb.st_size);
    close(md);
    line.append((char*)msg,sb.st_size);
    return(true);
  }

  void del_msg(uint32_t msid)
  { char filename[64];
    sprintf(filename,"ofi/msg_%08X.msd",msid);
    unlink(filename);
  }

  bool add_msg(uint8_t* msg,usertxs& utxs,uint32_t& msid,uint32_t& mpos)
  { assert(svid);
    if(!run){ return(false);}
    int len=utxs.size;
    file_.lock();
    if(message_tnum>=MESSAGE_TNUM_MAX){
      ELOG("MESSAGE busy, delaying message addition\n");}
    while(message_tnum>=MESSAGE_TNUM_MAX){
      file_.unlock();
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
      file_.lock();}
    msid=srv_.msid_+1; // check if no conflict !!!
    char filename[64];
    sprintf(filename,"ofi/msg_%08X.msd",msid);
    int md=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644);
    if(md<0){
      msid=0;
      mpos=0;
      file_.unlock();
      ELOG("ERROR, failed to log message %s\n",filename);
      return(false);} // :-( maybe we should throw here something
    write(md,msg,len);
    close(md);
    //mpos=message.length()+message::data_offset;
    mpos= ++message_tnum; //mpos is now the number of the transaction in the message, starts with 1 !!!
    message.append((char*)msg,len);
    if(utxs.ttype==TXSTYPE_SUS || utxs.ttype==TXSTYPE_UUS){
      user_t u;
      lseek(offifd_,utxs.buser*sizeof(user_t),SEEK_SET);
      read(offifd_,&u,sizeof(user_t));
      if(!u.msid){
        file_.unlock();
        return(false);}
      uint16_t oldstatus=u.stat;
      if(utxs.ttype==TXSTYPE_SUS){
        u.stat|=  (uint16_t)utxs.tmass & 0xFFFE ;} // can not change USER_STAT_DELETED
      else if(utxs.ttype==TXSTYPE_UUS){
        u.stat&=~((uint16_t)utxs.tmass & 0xFFFE);} // can not change USER_STAT_DELETED
      if(oldstatus!=u.stat){
        lseek(offifd_,-sizeof(user_t),SEEK_CUR);
        write(offifd_,&u,sizeof(user_t));}} // write only stat !!!
    file_.unlock();
    return(true);
  }

  bool lock_user(uint32_t cuser) //WARNING, ddos attack against user, use alternative protections
  { //users_[cuser & 0xff].lock();
    if(!svid){
      return(true);}
    users_.lock();
    if(users_lock.size()>CLIENT_POOL*2){
      users_.unlock();
      return(false);}
    if(users_lock.find(cuser)!=users_lock.end()){
      users_.unlock();
      return(false);}
    users_lock.insert(cuser);
    users_.unlock();
    return(true);
  }

  void unlock_user(uint32_t cuser)
  { //users_[cuser & 0xff].unlock();
    if(!svid){
      return;}
    users_.lock();
    users_lock.erase(cuser);
    users_.unlock();
    return;
  }

  void start_accept(); // main.cpp
  void handle_accept(client_ptr c,const boost::system::error_code& error); // main.cpp, currently blocking :-(
  void leave(client_ptr c); // main.cpp

  bool check_user(uint16_t peer,uint32_t uid)
  { if(peer!=svid){
      return(srv_.last_srvs_.check_user(peer,uid));} //use last_srvs_ for safety
    return(uid<users || !svid);
  }

  bool find_key(uint8_t* pkey,uint8_t* skey)
  { return(srv_.last_srvs_.find_key(pkey,skey));
  }

  uint32_t last_path()
  { return(srv_.last_srvs_.now);
  }

  uint32_t last_nodes()
  { return(srv_.last_srvs_.nodes.size());
  }

  uint32_t last_users(uint32_t bank)
  { if(bank>=srv_.last_srvs_.nodes.size()){
      return(0);}
    return(srv_.last_srvs_.nodes[bank].users);
  }

//LOG handling

//FIXME, log handling should go to office.hpp or better to log.hpp
  void mklogdir(uint16_t svid)
  { assert(svid);
    char filename[64];
    sprintf(filename,"log/%04X",svid);
    mkdir("log",0755); // create dir for user history
    mkdir(filename,0755);
  }

  // use (2) fallocate to remove beginning of files
  // Remove page A: ret = fallocate(fd, FALLOC_FL_COLLAPSE_RANGE, 0, 4096);
#ifdef FALLOC_FL_COLLAPSE_RANGE
  int purge_log(int fd,uint32_t user) // this is ext4 specific !!!
  { if(!svid){
      return(0);}
    log_t log;
    assert(!(4096%sizeof(log_t)));
    int r,size=lseek(fd,0,SEEK_END); // maybe stat would be faster
    if(size%sizeof(log_t)){
      ELOG("ERROR, log corrupt register\n");
      return(-2);}
    if(size<MIN_LOG_SIZE){
      return(0);}
    size=lseek(fd,0,SEEK_END); // just in case of concurrent purge
    for(;size>=MIN_LOG_SIZE;size-=4096){
      if(lseek(fd,4096-sizeof(log_t),SEEK_SET)!=4096-sizeof(log_t)){
        ELOG("ERROR, log lseek error\n");
        return(-1);}
      if((r=read(fd,&log,sizeof(log_t)))!=sizeof(log_t)){
        DLOG("ERROR, log read error (%d,%s)\n",r,strerror(errno));
        return(-1);}
      if(log.time+MAX_LOG_AGE>=srv_.last_srvs_.now){ // purge first block
        return(0);}
      if((r=fallocate(fd,FALLOC_FL_COLLAPSE_RANGE,0,4096))<0){
        DLOG("ERROR, log purge failed (%d,%s)\n",r,strerror(errno));
        return(-1);}}
    return(0);
  }
#else
  int purge_log(int fd,uint32_t user) // no purging on incompatible systems
  { return(0);
  }
#endif

#ifdef NOLOG
  void mklogfile(uint16_t svid,uint32_t user) {}
  void put_log(uint32_t user,log_t& log) {}
  void put_log(std::map<uint64_t,log_t>& log,uint32_t ntime) {}
#else
  void mklogfile(uint16_t svid,uint32_t user)
  { assert(svid);
    char filename[64];
    sprintf(filename,"log/%04X/%03X",svid,user>>20);
    mkdir(filename,0755);
    sprintf(filename,"log/%04X/%03X/%03X",svid,user>>20,(user&0xFFF00)>>8);
    mkdir(filename,0755);
    //create log file ... would be created later anyway
    sprintf(filename,"log/%04X/%03X/%03X/%02X.log",svid,user>>20,(user&0xFFF00)>>8,(user&0xFF));
    int fd=open(filename,O_WRONLY|O_CREAT,0644);
    if(fd<0){
      ELOG("ERROR, failed to create log directory %s\n",filename);}
    close(fd);
  }
  void put_log(uint32_t user,log_t& log) //single user, called by office and client
  { assert(svid);
    char filename[64];
    sprintf(filename,"log/%04X/%03X/%03X/%02X.log",svid,user>>20,(user&0xFFF00)>>8,(user&0xFF));
    int fd=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644);
    if(fd<0){
      mklogfile(svid,user);
      fd=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644);
      if(fd<0){
        ELOG("ERROR, failed to open log register %s\n",filename);
        return;}} // :-( maybe we should throw here something
    write(fd,&log,sizeof(log_t)); // lock() here
    close(fd);
  }
  void put_log(std::map<uint64_t,log_t>& log,uint32_t ntime) //many users, called by office and client
  { assert(svid);
    uint32_t luser=MAX_USERS;
    int fd=-1;
    if(log.empty()){
      return;}
    for(auto it=log.begin();it!=log.end();it++){
      uint32_t user=(it->first)>>32;
      if(luser!=user){
        if(fd>=0){
          purge_log(fd,luser);
          close(fd);}
        luser=user;
        char filename[64];
        sprintf(filename,"log/%04X/%03X/%03X/%02X.log",svid,user>>20,(user&0xFFF00)>>8,(user&0xFF));
        fd=open(filename,O_RDWR|O_CREAT|O_APPEND,0644); //maybe no lock needed with O_APPEND
        if(fd<0){
          mklogfile(svid,user);
          fd=open(filename,O_RDWR|O_CREAT|O_APPEND,0644); //maybe no lock needed with O_APPEND
          if(fd<0){
            ELOG("ERROR, failed to open log register %s\n",filename);
            return;}} // :-( maybe we should throw here something
        }
      it->second.time=ntime;
      write(fd,&it->second,sizeof(log_t));} // lock() here
    if(fd>=0){
      purge_log(fd,luser);
      close(fd);}
    log.clear();
  }
#endif

  void put_ulog(uint32_t user,log_t& log) //single user, called by client
  { assert(svid);
    log_.lock();
    log.time=time(NULL);
    put_log(user,log);
    log_.unlock();
  }

  void put_ulog(std::map<uint64_t,log_t>& log) //single user, called by client
  { assert(svid);
    log_.lock();
    uint32_t ntime=time(NULL);
    put_log(log,ntime);
    log_.unlock();
  }

  bool fix_log(uint16_t svid,uint32_t user)
  { assert(svid);
    char filename[64];
    struct stat sb;
    sprintf(filename,"log/%04X/%03X/%03X/%02X.log",svid,user>>20,(user&0xFFF00)>>8,(user&0xFF));
    int rd=open(filename,O_RDONLY|O_CREAT,0644); //maybe no lock needed with O_APPEND
    if(rd<0){
      ELOG("ERROR, failed to open log register %s for reading\n",filename);
      return(false);}
    fstat(rd,&sb);
    if(!(sb.st_size%sizeof(log_t))){ // file is ok
      close(rd);
      return(false);}
    int l=lseek(rd,sb.st_size%sizeof(log_t),SEEK_SET);
    int ad=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644); //maybe no lock needed with O_APPEND
    if(ad<0){
      ELOG("ERROR, failed to open log register %s for appending\n",filename);
      close(rd);
      return(false);}
    log_t log;
    memset(&log,0,sizeof(log_t));
    write(ad,&log,l); // append a suffix
    close(ad);
    int wd=open(filename,O_WRONLY|O_CREAT,0644); //maybe no lock needed with O_APPEND
    if(wd<0){
      ELOG("ERROR, failed to open log register %s for writing \n",filename);
      close(rd);
      return(false);}
    fstat(wd,&sb);
    if(sb.st_size%sizeof(log_t)){ // file is ok
      ELOG("ERROR, failed to append log register %s, why ?\n",filename);
      close(rd);
      close(wd);
      return(false);}
    for(l=sizeof(log_t);l<sb.st_size;l+=sizeof(log_t)){
      if(read(rd,&log,sizeof(log_t))!=sizeof(log_t)){
        ELOG("ERROR, failed to read log register %s while fixing\n",filename);
        break;}
      if(write(wd,&log,sizeof(log_t))!=sizeof(log_t)){
        ELOG("ERROR, failed to write log register %s while fixing\n",filename);
        break;}}
    if(write(wd,&log,sizeof(log_t))!=sizeof(log_t)){
      ELOG("ERROR, failed to write log register %s while duplicating last log",filename);}
    close(rd);
    close(wd);
    return(false);
  }

  //bool get_lastlog(uint16_t svid,uint32_t user,log_t& slog)

  bool get_log(uint16_t svid,uint32_t user,uint32_t from,std::string& slog)
  { if(!svid){
      return(true);}
    char filename[64];
    struct stat sb;
    if(from==0xffffffff){
      fix_log(svid,user);}
    sprintf(filename,"log/%04X/%03X/%03X/%02X.log",svid,user>>20,(user&0xFFF00)>>8,(user&0xFF));
    int fd=open(filename,O_RDWR|O_CREAT,0644); //maybe no lock needed with O_APPEND
    if(fd<0){
      ELOG("ERROR, failed to open log register %s\n",filename);
      return(false);}
    fstat(fd,&sb);
    uint32_t len=sb.st_size;
    slog.append((char*)&len,4);
    if(len%sizeof(log_t)){
      ELOG("ERROR, log corrupt register %s\n",filename);
      from=0;} // ignore from 
    log_t log;
    int l,mis=0;
    for(uint32_t tot=0;(l=read(fd,&log,sizeof(log_t)))>0 && tot<len;tot+=l){
      if(log.time<from){
        mis+=l;
        continue;}
      slog.append((char*)&log,l);}
    len-=mis;
    slog.replace(0,4,(char*)&len,4);
    if(!(len%sizeof(log_t))){
      purge_log(fd,user);}
    close(fd);
    return(true);
  }

  uint8_t* node_pkey(uint16_t node)
  { if(node>=srv_.last_srvs_.nodes.size()){
      return(NULL);}
    return(srv_.last_srvs_.nodes[node].pk); // use an old key !!!
  }

  uint16_t svid;
  hash_t pkey; // local copy for managing updates
  std::stack<gup_t> gup; // GET results
private:
  char ofifilename[64];
  const user_t user_tmp={};
  bool run;
  uint32_t div_ready;
  uint32_t block_ready;
  uint32_t users; //number of users of the bank
  std::string message;
//std::vector<int64_t> deposit; //resizing will require a stop of processing
//std::vector<int16_t> ustatus; //resizing will require a stop of processing
  boost::asio::ip::tcp::endpoint endpoint_;
  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  boost::asio::ip::tcp::acceptor acceptor_;
  server& srv_;
  options& opts_;
  std::set<client_ptr> clients_;
  int offifd_; // user file descriptor
  //uint32_t msid;
  //int64_t mfee;
  uint32_t message_sent;
  uint32_t message_tnum; // last transaction number
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
  std::deque<uint32_t> deleted_users; // list of accounts to reuse ... this list could be limited (swapped)

  boost::thread* ioth_;
  boost::thread* clock_thread;

  std::set<uint32_t> users_lock;

  boost::mutex file_; //LOCK: server::
  boost::mutex log_; //FIXME, maybe no need for this lock, lock too long
  boost::mutex users_; //LOCK: file_, log_
  boost::mutex client_;
  boost::mutex account_;
};

#endif // OFFICE_HPP
