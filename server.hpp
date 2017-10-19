#ifndef SERVER_HPP
#define SERVER_HPP

class office;
class peer;
typedef boost::shared_ptr<peer> peer_ptr;

class server
{
public:
  //server(boost::asio::io_service& io_service,const boost::asio::ip::tcp::endpoint& endpoint,options& opts) :
  server(options& opts) :
    do_sync(1),
    do_fast(opts.fast?2:0),
    ofip(NULL),
    endpoint_(boost::asio::ip::tcp::v4(),opts.port),	//TH
    io_service_(),
    work_(io_service_),
    acceptor_(io_service_,endpoint_),
    opts_(opts),
    ioth_(NULL),
    peers_thread(NULL),
    clock_thread(NULL),
    do_validate(0),
    votes_max(0.0),
    do_vote(0),
    do_block(0),
    block_only(true)
  {
  }

  ~server()
  { //do_validate=0;
    //threadpool.join_all();
    //clock_thread->interrupt();
    //clock_thread->join();
    ELOG("Server down\n");
  }

  void start()
  { mkdir("usr",0755); // create dir for bank accounts
    mkdir("inx",0755); // create dir for bank message indeces
    mkdir("blk",0755); // create dir for blocks
    mkdir("vip",0755); // create dir for blocks
    uint32_t path=readmsid()-opts_.back*BLOCKSEC; // reads msid_ and path, FIXME, do not read msid, read only path
    uint32_t lastpath=path;
    //remember start status
    start_path=path;
    start_msid=msid_;
    last_srvs_.get(path);
    if(!last_srvs_.nodes.size()){
      if(!opts_.init){
        ELOG("ERROR reading servers (size:%d)\n",(int)last_srvs_.nodes.size());
        exit(-1);}
      ELOG("CREATING first node\n");}
    else if((int)last_srvs_.nodes.size()<=(int)opts_.svid){
      ELOG("ERROR reading servers (size:%d)\n",(int)last_srvs_.nodes.size());
      exit(-1);}
    if(last_srvs_.nodes.size()){
      last_srvs_.update_vipstatus();
      bank_fee.resize(last_srvs_.nodes.size());}

    if(opts_.init){
      struct stat sb;   
      uint32_t now=time(NULL);
      now-=now%BLOCKSEC;
      if(stat("usr/0001.dat",&sb)>=0){
        if(!last_srvs_.nodes.size()){
          ELOG("ERROR reading servers for path %08X\n",path);
          exit(-1);}
        ELOG("INIT from last state @ %08X with MSID: %08X (file usr/0001.dat found)\n",path,msid_);
        if(!undo_bank()){
          ELOG("ERROR loading initial database, fatal\n");
          exit(-1);}
        // do not rebuild blockchain
        //last_srvs_.now=now;
        //last_srvs_.blockdir();
        //last_srvs_.finish();
        // rebuild blockchain
        srvs_=last_srvs_;
        memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
        period_start=srvs_.nextblock();
        ELOG("MAKE BLOCKCHAIN\n");
        for(;srvs_.now<now;){
          message_map empty;
          srvs_.msg=0;
          srvs_.msgl_put(empty,NULL);
          finish_block();}}
      else{
        path=0;
        lastpath=0;
        start_path=0;
        start_msid=0;
        msid_=0;
        ELOG("START from a fresh database\n");
        last_srvs_.init(now-BLOCKSEC);
        srvs_=last_srvs_;
        memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
        period_start=srvs_.nextblock(); //changes now!
        bank_fee.resize(last_srvs_.nodes.size());}
      if(!do_fast){ //always sync on do_fast
        do_sync=0;}}
    else{
      srvs_=last_srvs_;
      memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
      period_start=srvs_.nextblock();} //changes now!

    ELOG("START @ %08X with MSID: %08X\n",path,msid_);
    //vip_max=srvs_.update_vip(); //based on initial weights at start time, move to nextblock()

    if(last_srvs_.nodes.size()<=(unsigned)opts_.svid){ 
      ELOG("ERROR: reading servers (<=%d)\n",opts_.svid);
      exit(-1);} 
    iamvip=(bool)(srvs_.nodes[opts_.svid].status & SERVER_VIP);
    pkey=srvs_.nodes[opts_.svid].pk;
DLOG("INI:%016lX\n",*(uint64_t*)pkey);
    if(!last_srvs_.find_key(pkey,skey)){
      char pktext[2*32+1]; pktext[2*32]='\0';
      ed25519_key2text(pktext,pkey,32);
      ELOG("ERROR: failed to find secret key for key:\n%.64s\n",pktext);
      exit(-1);}

    if(!opts_.init){
      if(!undo_bank()){//check database consistance
        if(!do_fast){
          ELOG("DATABASE check failed, must use fast option to load new datase from network\n");
          exit(-1);}
        else{
          DLOG("DATABASE check failed, load new datase from network\n");}
        do_sync=1;}
      else{
        ELOG("DATABASE check passed\n");
        uint32_t now=time(NULL);
        now-=now%BLOCKSEC;
        if(last_srvs_.now==now-BLOCKSEC && !do_fast){
          do_sync=0;}
        else{
          if(last_srvs_.now<now-(BLOCKSEC*MAX_UNDO) && !do_fast){
            ELOG("WARNING, possibly missing too much history for full resync\n");}
          do_sync=1;}}}
    //ofip_init(last_srvs_.nodes[opts_.svid].users); //must do this after recycling messages !!!

    //FIXME, move this to a separate thread that will keep a minimum number of connections
    ioth_ = new boost::thread(boost::bind(&server::iorun, this));
    //for(std::string addr : opts_.peer){
    //  connect(addr);
    //  boost::this_thread::sleep(boost::posix_time::seconds(1)); //wait some time before connecting to more peers
    //  RETURN_ON_SHUTDOWN();}
    peers_thread = new boost::thread(boost::bind(&server::peers, this));

    if(do_sync){
      //if(opts_.fast){ //FIXME, do slow sync after fast sync
      if(do_fast){ //FIXME, do slow sync after fast sync
        while(do_fast>1){ // fast_sync changes the status, FIXME use future/promis
          boost::this_thread::sleep(boost::posix_time::seconds(1));
          //boost::this_thread::sleep(boost::posix_time::milliseconds(50));
          RETURN_ON_SHUTDOWN();}
        do_fast=0;
        //wait for all user files to arrive
        load_banks();
	srvs_.write_start();}
      //else{
      //  do_fast=0;}
      ELOG("START syncing headers\n");
      load_chain(); // sets do_sync=0;
      //svid_msgs_.clear();
      }
    //load old messages or check/modify
    //pkey=srvs_.nodes[opts_.svid].pk; //FIXME, is this needed ? srvs_ is overwritten in fast sync :-(

    RETURN_ON_SHUTDOWN();
    recyclemsid(lastpath+BLOCKSEC);
    RETURN_ON_SHUTDOWN();
    writemsid(); // synced to new position
    clock_thread = new boost::thread(boost::bind(&server::clock, this));
    start_accept();
  }

  void iorun()
  { while(1){
      try{
        DLOG("Server.Run starting\n");
        io_service_.run();
        DLOG("Server.Run finished\n");
        return;} //Now we know the server is down.
      catch (std::exception& e){
        ELOG("Server.Run error: %s\n",e.what());}}
  }
  void stop()
  { do_validate=0;
    io_service_.stop();
    if(ioth_!=NULL){
      ioth_->join();}
    threadpool.join_all();
    busy_msgs_.clear(); // not needed
    if(peers_thread!=NULL){
      peers_thread->interrupt();
      peers_thread->join();}
    if(clock_thread!=NULL){
      clock_thread->interrupt();
      clock_thread->join();}
    DLOG("Server shutdown completed\n");
  }

  void recyclemsid(uint32_t lastpath)
  { uint32_t firstmsid=srvs_.nodes[opts_.svid].msid;
    hash_t msha;
    if(firstmsid>msid_){
      ELOG("ERROR initial msid lower than on network, fatal (%08X<%08X)\n",msid_,firstmsid);
      if(!do_fast){
        exit(-1);}
      msid_=firstmsid;
      return;}
    if(firstmsid==msid_){
      DLOG("NO recycle needed\n");
      return;}
    DLOG("START recycle\n");
    memcpy(msha,srvs_.nodes[opts_.svid].msha,sizeof(hash_t));
    firstmsid++;
    uint32_t ntime=0;
    for(uint32_t lastmsid=firstmsid;lastmsid<=msid_;lastmsid++){
      message_ptr msg(new message(MSGTYPE_MSG,lastpath,opts_.svid,lastmsid,pkey,msha)); //load from file
      if(!(msg->status & MSGSTAT_DAT)){
        ELOG("ERROR, failed to read message %08X/%02x_%04x_%08x.msg\n",
          lastpath,MSGTYPE_MSG,opts_.svid,lastmsid);
        msid_=lastmsid-1;
        return;}
      if(msg->now<last_srvs_.now || ntime){ //FIXME, must sign this message again if message too old !!!
        //TODO, check double spend 
        if(!ntime){
	  ntime=time(NULL);
          assert(ntime>=srvs_.now);}
        ELOG("RECYCLED message %04X:%08X from %08X/ signing with new time %08X [len:%d]\n",opts_.svid,lastmsid,lastpath,ntime,msg->len);
        msg->signnewtime(ntime,skey,pkey,msha);
        ntime++;}
      memcpy(msha,msg->sigh,sizeof(hash_t));
      if(txs_insert(msg)){
        ELOG("RECYCLED message %04X:%08X from %08X/ inserted\n",opts_.svid,lastmsid,lastpath);
	if(srvs_.now!=lastpath){
          ELOG("MOVE message %04X:%08X from %08X/ to %08X/\n",opts_.svid,lastmsid,lastpath,srvs_.now);
          msg->move(srvs_.now);}}
      else{
        ELOG("RECYCLED message %04X:%08X from %08X/ known\n",opts_.svid,lastmsid,lastpath);}}
    if(msid_!=start_msid){
      ELOG("ERROR, failed to get correct msid now:%08X<>start:%08X, check msid.txt\n",msid_,start_msid);
      exit(-1);}
    //if(srvs_.now!=lastpath){
    //  message_ptr msg(new message());
    //  msg->path=lastpath;
    //  msg->hashtype(MSGTYPE_MSG);
    //  msg->svid=opts_.svid;
    //  for(;firstmsid<=msid_;firstmsid++){
    //    msg->msid=firstmsid;
    //    ELOG("REMOVING message %04X:%08X from %08X/\n",msg->svid,msg->msid,lastpath);
    //    msg->remove();}}
  }

  void del_msglog(uint32_t now,uint16_t svid,uint32_t msid)
  { char filename[64];
    sprintf(filename,"blk/%03X/%05X/log/%04X_%08X.log",now>>20,now&0xFFFFF,svid,msid);
    unlink(filename);
  }

  void put_msglog(uint32_t now,uint16_t svid,uint32_t msid,std::map<uint64_t,log_t>& log) //message log, by server
  { char filename[64];
    int fd;
    if(msid){
      sprintf(filename,"blk/%03X/%05X/log/%04X_%08X.log",now>>20,now&0xFFFFF,svid,msid);
      fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);}
    else{
      sprintf(filename,"blk/%03X/%05X/log/%04X_%08X.log",now>>20,now&0xFFFFF,svid,0);
      fd=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644);}
    if(fd<0){
      ELOG("ERROR, failed to open log file %s\n",filename);
      return;} // :-( maybe we should throw here something
    for(auto it=log.begin();it!=log.end();it++){
      uint32_t user=(it->first)>>32;
      write(fd,&user,sizeof(uint32_t));
      write(fd,&it->second,sizeof(log_t));}
    close(fd);
  }

  //update_nodehash is similar
  int undo_bank() //will undo database changes and check if the database is consistant
  { //could use multiple threads but disk access could limit the processing anyway
    //uint32_t path=srvs_.now; //use undo from next block
    uint32_t path=last_srvs_.now+BLOCKSEC; //use undo from next block
    int rollback=opts_.back;
    DLOG("CHECK DATA @%08X (and undo till @%08X)\n",last_srvs_.now,path+rollback*BLOCKSEC);
    bool failed=false;
    last_srvs_.blockdir();
    for(uint16_t bank=1;bank<last_srvs_.nodes.size();bank++){
      char filename[64];
      sprintf(filename,"usr/%04X.dat",bank);
      int fd=open(filename,O_RDWR);
      if(fd<0){
        failed=true;
        continue;}
      std::vector<int> ud;
      int n;
      for(n=0;n<=rollback;n++){
        uint32_t npath=path+n*BLOCKSEC;
        sprintf(filename,"blk/%03X/%05X/und/%04X.dat",npath>>20,npath&0xFFFFF,bank);
        int nd=open(filename,O_RDONLY);
        if(nd<0){
          continue;}
        DLOG("USING bank %04X block %08X undo %s\n",bank,npath,filename);
        ud.push_back(nd);}
      uint32_t users=last_srvs_.nodes[bank].users;
       int64_t weight=0;
      uint64_t csum[4]={0,0,0,0};
      for(uint32_t user=0;user<users;user++){
        user_t u;
        for(auto it=ud.begin();it!=ud.end();it++){
          u.msid=0;
          if(sizeof(user_t)==read(*it,&u,sizeof(user_t)) && u.msid){
            DLOG("OVERWRITE: %04X:%08X (weight:%016lX)\n",bank,user,u.weight);
            write(fd,&u,sizeof(user_t)); //overwrite bank file
            goto NEXTUSER;}}
        if(sizeof(user_t)!=read(fd,&u,sizeof(user_t))){
          ELOG("ERROR loading bank %04X (bad read)\n",bank);
          failed=true;}
        NEXTUSER:;
        weight+=u.weight;
        //FIXME, debug only !!!
#ifdef DEBUG
        user_t n;
        memcpy(&n,&u,sizeof(user_t));
        last_srvs_.user_csum(n,bank,user);
        if(memcmp(n.csum,u.csum,32)){
          ELOG("ERROR !!!, checksum mismatch for user %04X:%08X [%08X<>%08X]\n",bank,user,
            *((uint32_t*)(n.csum)),*((uint32_t*)(u.csum)));
          failed=true;}
#endif
        last_srvs_.xor4(csum,u.csum);}
      close(fd);
      for(auto nd : ud){
        close(nd);}
      if(last_srvs_.nodes[bank].weight!=weight){
        ELOG("ERROR loading bank %04X (bad sum:%016lX<>%016lX)\n",
          bank,last_srvs_.nodes[bank].weight,weight);
        failed=true;}
      if(memcmp(last_srvs_.nodes[bank].hash,csum,32)){
        ELOG("ERROR loading bank %04X (bad hash)\n",bank);
        failed=true;}}
    if(failed){
      return(0);}
    return(1);
  }

  void load_banks()
  {
    //create missing bank messages
    uint16_t end=last_srvs_.nodes.size();
    std::set<uint16_t> ready;
    peerready(ready); //get list of peers available for data download
    missing_.lock();
    missing_msgs_.clear();
    for(uint16_t bank=1;bank<end;bank++){
      //TODO, unlikely but maybe we have the correct bank already, we could check this
      message_ptr put_msg(new message());
      put_msg->data[0]=MSGTYPE_USG;
      put_msg->data[1]=0;
      memcpy(put_msg->data+2,&last_srvs_.now,4);
      memcpy(put_msg->data+6,&bank,2);
      put_msg->msid=last_srvs_.now;
      put_msg->svid=bank;
      put_msg->hash.num=put_msg->dohash(put_msg->data);
      put_msg->got=0; // do first request emidiately
      missing_msgs_[put_msg->hash.num]=put_msg;}
    uint32_t mynow=time(NULL);
    for(auto peer=ready.begin();peer!=ready.end();peer++){ // send 2 requestes to each peer
      for(auto pm=missing_msgs_.begin();pm!=missing_msgs_.end();pm++){
        uint32_t maxwait=last_srvs_.nodes[pm->second->svid].users*sizeof(user_t)/1000000; //expect 1Mb connection
        if(pm->second->cansend(*peer,mynow,maxwait)){
          DLOG("REQUESTING BANK %04X from %04X\n",pm->second->svid,*peer);
          deliver(pm->second,*peer); //LOCK: pio_
          DLOG("REQUESTING BANK %04X from %04X sent\n",pm->second->svid,*peer);
          break;}}}
    while(missing_msgs_.size()){
      uint32_t mynow=time(NULL);
      for(auto peer=ready.begin();peer!=ready.end();peer++){ // send 2 requestes to each peer
        for(auto pm=missing_msgs_.begin();pm!=missing_msgs_.end();pm++){
          uint32_t maxwait=last_srvs_.nodes[pm->second->svid].users*sizeof(user_t)/1000000; //expect 1Mb connection
          if(pm->second->cansend(*peer,mynow,maxwait)){ // wait dependend on message size !!!
            DLOG("REQUESTING BANK %04X from %04X\n",pm->second->svid,*peer);
            deliver(pm->second,*peer); //LOCK: pio_
            DLOG("REQUESTING BANK %04X from %04X sent\n",pm->second->svid,*peer);
            break;}}}
      missing_.unlock();
      ELOG("WAITING for banks (peers:%d)\n",(int)ready.size());
      //TODO sleep much shorter !!!
      boost::this_thread::sleep(boost::posix_time::seconds(1)); //yes, yes, use futur/promise instead
      RETURN_ON_SHUTDOWN();
      peerready(ready); //get list of peers available for data download
      missing_.lock();}
    missing_.unlock();
    //we should have now all banks
  }

  uint64_t need_bank(uint16_t bank) //FIXME, return 0 if not at this stage
  { union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=0;
    h.dat[1]=MSGTYPE_USR;
    memcpy(h.dat+2,&last_srvs_.now,4);
    memcpy(h.dat+6,&bank,2);
    missing_.lock();
    if(missing_msgs_.find(h.num)!=missing_msgs_.end()){
      missing_.unlock();
      return(h.num);}
    missing_.unlock();
    return(0);
  }

  void have_bank(uint64_t hnum)
  { missing_.lock();
    missing_msgs_.erase(hnum);
    missing_.unlock();
  }

  void load_chain()
  { 
    uint32_t now=time(NULL);
    now-=now%BLOCKSEC;
    do_validate=1;
    for(int v=0;v<VALIDATORS;v++){
      threadpool.create_thread(boost::bind(&server::validator, this));}
//FIXME, must start with a matching nowhash and load serv_
    if(srvs_.now<now){
      uint32_t n=headers.size();
      for(;!n;){
        get_more_headers(srvs_.now); // try getting more headers
        ELOG("\nWAITING 1s (%08X<%08X)\n",srvs_.now,now);
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        RETURN_ON_SHUTDOWN();
        n=headers.size();}
      auto block=headers.begin();
      for(;;){
        ELOG("START syncing header %08X\n",block->now);
        if(srvs_.now!=block->now){
          ELOG("ERROR, got strange block numbers %08X<>%08X\n",srvs_.now,block->now);
          exit(-1);} //FIXME, prevent this
        //block->load_signatures(); //TODO should go through signatures and update vok, vno
        block->header_put(); //FIXME will loose relation to signatures, change signature filename to fix this
        if(!block->msgl_load(missing_msgs_,opts_.svid)){
          ELOG("LOAD messages from peers\n");
          //request list of transactions from peers
          get_msglist=srvs_.now;
          //prepare txslist request message
          message_ptr put_msg(new message());
          put_msg->data[0]=MSGTYPE_MSL;
          memcpy(put_msg->data+1,&block->now,4);
          put_msg->got=0; // do first request emidiately
          while(get_msglist){ // consider using future/promise
            uint32_t nnow=time(NULL);
            if(put_msg->got<nnow-MAX_MSGWAIT){
              fillknown(put_msg); // do this again in case we have a new peer, FIXME, let the peer do this
              uint16_t svid=put_msg->request();
              if(svid){
                int ok=deliver(put_msg,svid);
                ELOG("REQUESTING MSL from %04X (%d)\n",svid,ok);}}
            boost::this_thread::sleep(boost::posix_time::milliseconds(50));
            RETURN_ON_SHUTDOWN();}
          srvs_.msg=block->msg; //check
          memcpy(srvs_.msghash,block->msghash,SHA256_DIGEST_LENGTH);}
        else{
          srvs_.msg=block->msg; //check
          memcpy(srvs_.msghash,block->msghash,SHA256_DIGEST_LENGTH);}
        //inform peers about current sync block
        message_ptr put_msg(new message());
        put_msg->data[0]=MSGTYPE_PAT;
        memcpy(put_msg->data+1,&srvs_.now,4);
        deliver(put_msg);
        //request missing messages from peers
        txs_.lock();
        txs_msgs_.clear();
        txs_.unlock();
        dbl_.lock(); //FIXME, there are no dbl_ messages in blocks !!!
        dbl_msgs_.clear();
        dbl_.unlock();
        ldc_.lock();
        ldc_msgs_.clear();
        ldc_.unlock();
        std::set<uint16_t> update;
        missing_.lock();
        LAST_block_final_msgs=missing_msgs_;
        //message_queue commit_msgs;
        for(auto it=missing_msgs_.begin();it!=missing_msgs_.end();){
          auto jt=it++;
          missing_.unlock();
          update.insert(jt->second->svid);
          txs_.lock();
          txs_msgs_[jt->first]=jt->second;
          txs_.unlock();
          if(jt->second->msid==0xFFFFFFFF){
            if(jt->second->status & MSGSTAT_VAL){
              srvs_.nodes[jt->second->svid].status |= SERVER_DBL;}
            missing_.lock();
            missing_msgs_.erase(jt);
            continue;}
          ldc_.lock();
          ldc_msgs_[jt->first]=jt->second;
          ldc_.unlock();
          if(jt->second->load(0)){ // will be unloaded by the validator
            if(!jt->second->sigh_check()){
              jt->second->read_head(); //to get 'now'
              DLOG("LOADING TXS %04X:%08X from path:%08X\n",
                jt->second->svid,jt->second->msid,jt->second->path);
              check_.lock();
              check_msgs_.push_back(jt->second); // send to validator
              check_.unlock();
              missing_.lock();
              missing_msgs_.erase(jt);
              continue;}
            DLOG("LOADING TXS %04X:%08X from path:%08X failed\n",
              jt->second->svid,jt->second->msid,jt->second->path);
            jt->second->len=message::header_length;}
          fillknown(jt->second);
          uint16_t svid=jt->second->request(); //FIXME, maybe request only if this is the next needed message, need to have serv_ ... ready for this check :-/
          if(svid){
            if(srvs_.nodes[jt->second->svid].msid==jt->second->msid-1){ // do not request if previous message not processed
              DLOG("REQUESTING TXS %04X:%08X from %04X\n",jt->second->svid,jt->second->msid,svid);
              deliver(jt->second,svid);}
            else{
              DLOG("POSTPONING TXS %04X:%08X\n",jt->second->svid,jt->second->msid);}}
          missing_.lock();}
        missing_.unlock();
        //wait for all messages to be processed by the validators
        ldc_.lock();
        while(ldc_msgs_.size()){
          ldc_.unlock();
          boost::this_thread::sleep(boost::posix_time::milliseconds(50)); //yes, yes, use futur/promise instead
          RETURN_ON_SHUTDOWN();
          ldc_.lock();}
        ldc_.unlock();
        //run save_mnum for each message (msgl_put ...)
        int n=0;
        for(auto it=LAST_block_final_msgs.begin();it!=LAST_block_final_msgs.end();it++){
          if(it->second->msid==0xFFFFFFFF){
            continue;}
          assert((it->second->status & MSGSTAT_SAV));
          assert((it->second->status & MSGSTAT_COM));
          assert((it->second->status & MSGSTAT_VAL));
          it->second->save_mnum(++n);}

//FIXME, txshash not calculated !!!

        //DLOG("TXSHASH: %08X\n",*((uint32_t*)srvs_.msghash));
        DLOG("COMMIT deposits\n");
        commit_block(update); // process bkn and get transactions
        commit_dividends(update);
        commit_deposit(update);
        commit_bankfee();
        DLOG("UPDATE accounts\n");
#ifdef DEBUG
        for(auto it=update.begin();it!=update.end();it++){
          assert(*it<srvs_.nodes.size());
          if(!srvs_.check_nodehash(*it)){ //FIXME, remove this later !, this is checked during download.
            ELOG("FATAL ERROR, failed to check the hash of bank %04X at block %08X\n",*it,srvs_.now);
            exit(-1);}}
#endif
        //finish block
        srvs_.finish(); //FIXME, add locking
        if(memcmp(srvs_.nowhash,block->nowhash,SHA256_DIGEST_LENGTH)){
          ELOG("ERROR, failed to arrive at correct hash at block %08X, fatal\n",srvs_.now);
          exit(-1);}
        last_srvs_=srvs_; // consider not making copies of nodes
        memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
        period_start=srvs_.nextblock();
        iamvip=(bool)(srvs_.nodes[opts_.svid].status & SERVER_VIP);
        //FIXME should be a separate thread
        DLOG("UPDATE LOG\n");
        ofip_update_block(period_start,0,LAST_block_final_msgs,srvs_.div);
        DLOG("PROCESS LOG\n");
        ofip_process_log(srvs_.now-BLOCKSEC);
        DLOG("WRITE NEW MSID\n");
        writemsid();
        now=time(NULL);
        now-=now%BLOCKSEC;
        if(srvs_.now>=now){
          break;}
        headers_.lock();
        for(block++;block==headers.end();block++){ // wait for peers to load more blocks
          block--;
          headers_.unlock();
          ELOG("WAITING at block end (headers:%d) (srvs_.now:%08X;now:%08X) \n",
            (int)headers.size(),srvs_.now,now);
          //FIXME, insecure !!! better to ask more peers / wait for block with enough votes
          get_more_headers(block->now+BLOCKSEC);
          boost::this_thread::sleep(boost::posix_time::seconds(2));
          RETURN_ON_SHUTDOWN();
          headers_.lock();}
        headers_.unlock();}}
    //TODO, add nodes if needed
    //vip_max=srvs_.update_vip(); // move to nextblock()
    txs_.lock();
    txs_msgs_.clear();
    txs_.unlock();
    dbl_.lock();
    dbl_msgs_.clear();
    dbl_.unlock();
    //FIXME, inform peers about sync status
    headers_.lock();
    do_sync=0;
    headers.clear();
    headers_.unlock();
    message_ptr put_msg(new message());
    put_msg->data[0]=MSGTYPE_SOK;
    memcpy(put_msg->data+1,&srvs_.now,4);
    deliver(put_msg);
  }

  //void put_msglist(uint32_t now,message_map& map)
  void msgl_process(servers& header,uint8_t* data)
  { missing_.lock(); // consider changing this to missing_lock
    if(get_msglist!=header.now){
      missing_.unlock();
      return;}
    message_map map;
    header.msgl_map((char*)data,map,opts_.svid);
    if(!header.msgl_put(map,(char*)data)){
      missing_.unlock();
      return;}
    missing_msgs_.swap(map);
    get_msglist=0;
    missing_.unlock();
    return;
  }

  void get_last_header(servers& sync_ls,handshake_t& sync_hs)
  { headers_.lock();
    if(headers.size()){
      sync_ls=headers.back();
      headers_.unlock();}
    else{
      headers_.unlock();
      sync_ls.loadhead(sync_hs.head);}
  }

  void add_next_header(uint32_t from,servers& peer_ls)
  { headers_.lock();
    if((!headers.size() &&
         !memcmp(peer_ls.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH)) ||
       (headers.back().now==from-BLOCKSEC &&
         !memcmp(peer_ls.oldhash,headers.back().nowhash,SHA256_DIGEST_LENGTH))){
      headers.insert(headers.end(),peer_ls);}
    headers_.unlock();
  }

  void add_headers(std::vector<servers>& peer_headers)
  { if(!do_sync){
      return;}
    headers_.lock();
    if(!headers.size()){
      headers.insert(headers.end(),peer_headers.begin(),peer_headers.end());
      headers_.unlock();
      return;}
    auto it=peer_headers.begin();
    for(;it!=peer_headers.end() && it->now<=headers.back().now;it++){}
    if(headers.back().now!=peer_headers.begin()->now-BLOCKSEC){
      ELOG("ERROR, headers misaligned\n"); //should never happen
      headers_.unlock();
      return;}
    headers.insert(headers.end(),it,peer_headers.end());
    headers_.unlock();
  }

  int fast_sync(bool done,header_t& head,node_t* nods,svsi_t* svsi)
  { static uint32_t last=0;
    for(;;){
      uint32_t now=time(NULL);
      headers_.lock();
      if(do_fast<2){
        headers_.unlock();
        return(-1);}
      if(done){ // peer should now overwrite servers with current data
        last_srvs_.overwrite(head,nods);
        last_srvs_.blockdir();
	last_srvs_.put();
	last_srvs_.put_signatures(head,svsi);
        srvs_=last_srvs_; //FIXME, create a copy function
        memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
        period_start=srvs_.nextblock();
        iamvip=(bool)(srvs_.nodes[opts_.svid].status & SERVER_VIP);
        pkey=srvs_.nodes[opts_.svid].pk; //FIXME, is this needed and safe ?
        do_fast=1;
        headers_.unlock();
        return(1);}
      if(last<now-SYNC_WAIT){
        last=now;
        headers_.unlock();
        return(1);}
      headers_.unlock();
      boost::this_thread::sleep(boost::posix_time::seconds(1));
      RETURN_VAL_ON_SHUTDOWN(0);}
    return 0;
  }

  uint32_t readmsid()
  { FILE* fp=fopen("msid.txt","r");
    if(fp==NULL){
      msid_=0;
      return(0);}
    uint32_t path;
    uint32_t svid;
    uint64_t *h=(uint64_t*)&msha_;
    fscanf(fp,"%X %X %X %lX %lX %lX %lX",&msid_,&path,&svid,h+3,h+2,h+1,h+0);
    fclose(fp);
    if(svid!=(uint32_t)opts_.svid){
      throw("FATAL ERROR: failed to read correct svid from msid.txt\n");}
    return(path);
  }

  //FIXME, move this to servers.hpp
  void writemsid()
  { FILE* fp=fopen("msid.txt","w");
    if(fp==NULL){
      throw("FATAL ERROR: failed to write to msid.txt\n");}
    uint64_t *h=(uint64_t*)&msha_;
    fprintf(fp,"%08X %08X %04X %016lX %016lX %016lX %016lX\n",msid_,last_srvs_.now,opts_.svid,h[3],h[2],h[1],h[0]);
    fclose(fp);
  }

  void clean_last_svid_msgs(std::map<uint16_t,message_ptr>& map) // remove !!!
  { for(std::map<uint16_t,message_ptr>::iterator jt=map.begin();jt!=map.end();){
      auto it=jt++;
      if(it->second->msid<=last_srvs_.nodes[it->first].msid){
        DLOG("CLEAN: %04X:%08X<-[%08X] !!!\n",it->first,it->second->msid,
          last_srvs_.nodes[it->first].msid);
        map.erase(it);}}
  }

  void message_shash(uint8_t* mhash,message_map& map)
  { SHA256_CTX sha256;
    SHA256_Init(&sha256);
    for(auto it=map.begin();it!=map.end();++it){
      char sigh[64];
      ed25519_key2text(sigh,it->second->sigh,32);
      DLOG("____ HASH %04X:%08X %016lX %.*s\n",it->second->svid,it->second->msid,it->first,64,sigh);
      //DLOG("____ HASH %04X:%08X<-%08X@%08X %.*s\n",it->second->svid,it->second->msid,
      //  last_srvs_.nodes[it->second->svid].msid,last_srvs_.now,2*SHA256_DIGEST_LENGTH,sigh);
      SHA256_Update(&sha256,it->second->sigh,4*sizeof(uint64_t));}
    SHA256_Final(mhash, &sha256);
  }

  void LAST_block_msgs()
  { LAST_block_svid_msgs.clear();
    LAST_block_all_msgs.clear();
    uint16_t lastsvid=0;
    uint32_t minmsid=0;
    uint32_t nowmsid=0; //test only
    txs_.lock();
    for(auto me=txs_msgs_.begin();me!=txs_msgs_.end();me++){ // process COM messages
      if(!(me->second->status & MSGSTAT_COM) ||
          (me->second->status & MSGSTAT_VAL)){
        continue;}
      if(lastsvid!=me->second->svid){
        lastsvid=me->second->svid;
        minmsid=last_srvs_.nodes[lastsvid].msid+1;
        nowmsid=minmsid;} //test only
      if(me->second->msid<minmsid){
        continue;}
      assert(me->second->msid==nowmsid); //test only
      nowmsid++; //test only
      LAST_block_all_msgs[me->first & 0xFFFFFFFFFFFF0000L]=me->second;
      LAST_block_svid_msgs[lastsvid]=me->second;}
    txs_.unlock();
    dbl_.lock();
    for(auto it=dbl_msgs_.begin();it!=dbl_msgs_.end();it++){
      uint16_t svid=it->second->svid;
      if(!(last_srvs_.nodes[svid].status & SERVER_DBL) && known_dbl(svid)){
        message_ptr msg(new message());
        msg->dblhash(svid);
        LAST_block_all_msgs[msg->hash.num]=msg;}}
    dbl_.unlock();
    LAST_block=srvs_.now;
  }

  void LAST_block_final(hash_s& cand)
  { assert(srvs_.now==LAST_block);
    message_map LAST_block_tmp_msgs=LAST_block_all_msgs;
    for(auto dm=winner->msg_del.begin();dm!=winner->msg_del.end();dm++){
      LAST_block_tmp_msgs.erase(*dm);}
    LAST_block_final_msgs.clear(); // keys NOT masked with 0xFFFFFFFFFFFF0000L !!!
    for(auto it=LAST_block_tmp_msgs.begin();it!=LAST_block_tmp_msgs.end();it++){
      it->second->status|=MSGSTAT_VAL;
      if(it->second->msid==0xFFFFFFFF){
        uint16_t svid=it->second->svid;
        ELOG("DOUBLE setting dbl status for node %04X\n",svid);
        dbls_.lock();
        dbl_srvs_.insert(svid); //FIXME, pointless
        dbls_.unlock();
        srvs_.nodes[svid].status |= SERVER_DBL;}
      assert(winner->msg_add.find(it->first)==winner->msg_add.end());
      //if(winner->msg_add.find(it->first)!=winner->msg_add.end()){
      //  continue;}
      LAST_block_final_msgs.insert(LAST_block_final_msgs.end(),
          std::pair<uint64_t,message_ptr>(it->second->hash.num,it->second));}
    LAST_block_tmp_msgs.clear();
    for(auto am=winner->msg_add.begin();am!=winner->msg_add.end();am++){
      if(*(uint32_t*)(((uint8_t*)&am->first)+2)==0xFFFFFFFF){
        uint16_t svid=*(uint16_t*)(((uint8_t*)&am->first)+6);
        ELOG("DOUBLE setting dbl status for node %04X\n",svid);
        dbls_.lock();
        dbl_srvs_.insert(svid); //FIXME, pointless
        dbls_.unlock();
        srvs_.nodes[svid].status |= SERVER_DBL;}
      message_ptr msg(new message((uint16_t*)(((uint8_t*)&am->first)+6),(uint32_t*)(((uint8_t*)&am->first)+2),
          (char*)am->second.hash,opts_.svid,LAST_block));
      msg->status|=MSGSTAT_VAL;
      LAST_block_final_msgs[msg->hash.num]=msg;}
    txs_.lock();
    char filename[64];
    sprintf(filename,"blk/%03X/%05X/delta.txt",LAST_block>>20,LAST_block&0xFFFFF);
    FILE *fp=fopen(filename,"w");
    char hash[64];
    //add new messages
    auto lm=LAST_block_final_msgs.begin();
    auto tm=txs_msgs_.begin();
    message_map txs_msgs_add;
    std::set<message_ptr> recover;
    for(;lm!=LAST_block_final_msgs.end();){
      if(tm==txs_msgs_.end() || lm->first<tm->first){
        if(tm!=txs_msgs_.end() && !((tm->first ^ lm->first) & 0xFFFFFFFFFFFF0000L)){
          if(!(tm->second->status & MSGSTAT_BAD)){
            bad_insert(tm->second);}}
        assert(txs_msgs_.find(lm->first)==txs_msgs_.end());
        if(lm->second->path && lm->second->path!=LAST_block){
          lm->second->move(LAST_block);}
        lm->second->path=LAST_block;
        //FIXME :-( no info about peer inventory :-(
        txs_msgs_add[lm->first]=lm->second;
        ed25519_key2text(hash,lm->second->sigh,sizeof(hash_t));
        fprintf(fp,"%04X:%08X %.*s new\n",lm->second->svid,lm->second->msid,64,hash);
        lm++;
        continue;}
      if(lm->first==tm->first){
        if(!(tm->second->status & MSGSTAT_DAT)){
          tm->second->status|=MSGSTAT_VAL;
          memcpy(tm->second->sigh,lm->second->sigh,32);}
        else if(memcmp(tm->second->sigh,lm->second->sigh,32)){
          if((tm->second->status & (MSGSTAT_BAD|MSGSTAT_DAT))==MSGSTAT_DAT){
            bad_insert(tm->second);}
          //FIXME :-( overwrites info about peer inventory :-(
          tm->second=lm->second;}
        if(tm->second->status & MSGSTAT_BAD){
          recover.insert(tm->second);}
        tm->second->status|=MSGSTAT_VAL;
        if(tm->second->path && tm->second->path!=LAST_block){
          tm->second->move(LAST_block);}
        tm->second->path=LAST_block;
        ed25519_key2text(hash,tm->second->sigh,sizeof(hash_t));
        fprintf(fp,"%04X:%08X %.*s\n",lm->second->svid,lm->second->msid,64,hash);
        tm++; //ERROR, must compare new tm to previous lm too !!!
        while(tm!=txs_msgs_.end() && !((tm->first ^ lm->first) & 0xFFFFFFFFFFFF0000L)){
          if(!(tm->second->status & MSGSTAT_BAD)){
            bad_insert(tm->second);}
          tm++;}
        lm++;
        continue;}
      while(tm!=txs_msgs_.end() && lm->first>tm->first){
        if(!((tm->first ^ lm->first) & 0xFFFFFFFFFFFF0000L)){
          if(!(tm->second->status & MSGSTAT_BAD)){
            bad_insert(tm->second);}}
        tm++;}}
    fclose(fp);
    if(LAST_block_final_msgs.size()>0){
      lm--;
      while(tm!=txs_msgs_.end() && !((tm->first ^ lm->first) & 0xFFFFFFFFFFFF0000L)){
        if(!(tm->second->status & MSGSTAT_BAD)){
          bad_insert(tm->second);}
        tm++;}}
    txs_msgs_.insert(txs_msgs_add.begin(),txs_msgs_add.end());
    for(auto me : recover){ // must do this after removing all BAD messages
      assert(me->status & MSGSTAT_VAL);
      bad_recover(me);}
    //srvs_.msg=LAST_block_final_msgs.size(); //done in msgl_put
    srvs_.msgl_put(LAST_block_final_msgs,NULL);
    hash_s last_block_message;
    message_shash(last_block_message.hash,LAST_block_final_msgs);
    if(memcmp(last_block_message.hash,cand.hash,32)){
      char tex1[2*SHA256_DIGEST_LENGTH];
      char tex2[2*SHA256_DIGEST_LENGTH];
      ed25519_key2text(tex1,last_block_message.hash,SHA256_DIGEST_LENGTH);
      ed25519_key2text(tex2,cand.hash,SHA256_DIGEST_LENGTH);
      ELOG("FATAL hash mismatch\n%.64s\n%.64s\nblk/%03X/%05X/log.txt\n",
        tex1,tex2,srvs_.now>>20,srvs_.now&0xFFFFF);
      exit(-1);}
    wait_.lock(); //maybe not needed if no validators
    wait_msgs_.insert(wait_msgs_.begin(),check_msgs_.begin(),check_msgs_.end());
    wait_.unlock();
    check_.lock(); //maybe not needed if no validators
    check_msgs_.clear();
    check_.unlock();

    //clean and undo txs messages
    std::deque<message_ptr> signnew; // vector of own late message for time update, signature and resubmission
    uint16_t lastsvid=0;
    uint32_t minmsid=0;
    if(!txs_msgs_.empty()){
    auto tn=txs_msgs_.end();
    for(tn--;tn!=txs_msgs_.end();){ // must use forward iterator in erase
      auto tm=tn;
      if(tn==txs_msgs_.begin()){
        tn=txs_msgs_.end();}
      else{
        tn--;}
      if(tm->second->svid!=lastsvid){
        lastsvid=tm->second->svid;
        if(last_srvs_.nodes.size()<=lastsvid){
          minmsid=last_srvs_.nodes[lastsvid].msid;}
        else{
          minmsid=0;}}
      if(tm->second->msid<=minmsid){
        ELOG("FORGET message %04X:%08X [min:%08X len:%d]\n",tm->second->svid,tm->second->msid,minmsid,tm->second->len);
        tm->second->remove_undo();
        txs_msgs_.erase(tm);
        continue;}
      if((tm->second->status & MSGSTAT_BAD)){
        ELOG("REMOVE bad message %04X:%08X [min:%08X len:%d]\n",tm->second->svid,tm->second->msid,minmsid,tm->second->len);
        if((tm->second->status & MSGSTAT_COM)){
          undo_message(tm->second);}
        //bad_insert(tm->second); ... already inserted
        remove_message(tm->second);
        txs_msgs_.erase(tm);
        continue;}
      if(!(tm->second->status & MSGSTAT_VAL) && (tm->second->status & MSGSTAT_COM)){
        ELOG("UNDO message %04X:%08X [min:%08X len:%d status:%X]\n",tm->second->svid,tm->second->msid,minmsid,tm->second->len,tm->second->status);
        undo_message(tm->second);
        //if(tm->second->now<last_srvs_.now) will be too late :-(
        if(tm->second->now<srvs_.now){
          ELOG("REMOVE late message %04X:%08X [min:%08X len:%d]\n",tm->second->svid,tm->second->msid,minmsid,tm->second->len);
          message_ptr msg=tm->second;
          bad_insert(tm->second);
          remove_message(tm->second);
          txs_msgs_.erase(tm);
          if(msg->svid==opts_.svid){
            signnew.push_front(msg);}}
        else{
          ELOG("INVALIDATE message %04X:%08X [min:%08X len:%d]\n",tm->second->svid,tm->second->msid,minmsid,tm->second->len);
          tm->second->move(LAST_block+BLOCKSEC);
          wait_.lock();
          wait_msgs_.push_back(tm->second);
          wait_.unlock();}
        continue;}
      if(!(tm->second->status & MSGSTAT_VAL) && tm->second->path && tm->second->path<=LAST_block){
        //if(tm->second->now<last_srvs_.now) will be too late :-(
        if(tm->second->now<last_srvs_.now){
          ELOG("REMOVE late message %04X:%08X [min:%08X len:%d]\n",tm->second->svid,tm->second->msid,minmsid,tm->second->len);
          message_ptr msg=tm->second;
          bad_insert(tm->second);
          remove_message(tm->second);
          txs_msgs_.erase(tm);
          if(msg->svid==opts_.svid){
            signnew.push_front(msg);}}
        else{
          ELOG("MOVE message %04X:%08X [min:%08X len:%d]\n",tm->second->svid,tm->second->msid,minmsid,tm->second->len);
          tm->second->move(LAST_block+BLOCKSEC);}}
      if((tm->second->status & (MSGSTAT_VAL | MSGSTAT_COM)) == MSGSTAT_VAL ){
        if(tm->second->msid==0xFFFFFFFF){
          ELOG("COMMIT dbl message %04X:%08X [len:%d]\n",tm->second->svid,tm->second->msid,tm->second->len);
          tm->second->status|=MSGSTAT_COM; // the only place to commit dbl messages
          assert(srvs_.nodes[tm->second->svid].status&SERVER_DBL);
          continue;}
        if(tm->second->status & MSGSTAT_DAT){
          ELOG("QUEUE message %04X:%08X [len:%d]\n",tm->second->svid,tm->second->msid,tm->second->len);
          wait_.lock();
          for(auto wm=wait_msgs_.begin();wm!=wait_msgs_.end();wm++){
            if((*wm)==tm->second){
              wait_msgs_.erase(wm);
                break;}}
          wait_.unlock();
          //the queue is not efficient this way, because of consecutive tasks from same node
          check_.lock(); //maybe not needed if no validators
          check_msgs_.push_front(tm->second);
          check_.unlock();
          continue;}
        //check if missing messages where not stored as bad
	hash_s* hash_p=(hash_s*)tm->second->sigh;
        auto it=bad_msgs_.find(*hash_p); //no lock here
        //auto it=bad_msgs_.find(*(hash_s*)tm->second->sigh);
        if(it!=bad_msgs_.end()){
          if(it->second->hash.num==tm->second->hash.num){
            ELOG("RECOVER message %04X:%08X [len:%d]\n",tm->second->svid,tm->second->msid,tm->second->len);
            tm->second=it->second;
            assert(tm->second->status & MSGSTAT_DAT);
            bad_recover(tm->second);
            tm->second->move(LAST_block);
            tm->second->status|=MSGSTAT_VAL;
            wait_.lock();
            for(auto wm=wait_msgs_.begin();wm!=wait_msgs_.end();wm++){
              if((*wm)==tm->second){
                wait_msgs_.erase(wm);
                  break;}}
            wait_.unlock();
            check_.lock(); //maybe not needed if no validators
            check_msgs_.push_front(tm->second);
            check_.unlock();
            continue;}}
        //FIXME, check info about peer inventory !!! find out who knows about the message !!!
        ELOG("MISSING message %04X:%08X [len:%d]\n",tm->second->svid,tm->second->msid,tm->second->len);
        missing_msgs_[tm->second->hash.num]=tm->second;}}
    }

    if(!signnew.empty()){ // sign again messages that failed to be accepted by the network on time
      uint32_t ntime=time(NULL);
      uint32_t msid=srvs_.nodes[opts_.svid].msid;
      hash_t msha;
      memcpy(msha,srvs_.nodes[opts_.svid].msha,sizeof(hash_t));
      for(auto mp=signnew.begin();mp!=signnew.end();mp++){
        msid++;
        assert(msid==(*mp)->msid);
        (*mp)->load(0);
        (*mp)->signnewtime(ntime,skey,pkey,msha);
        (*mp)->status &= ~MSGSTAT_BAD;
        memcpy(msha,(*mp)->sigh,sizeof(hash_t));
        (*mp)->save();
        (*mp)->unload(0);
        check_.lock(); //maybe not needed if no validators
        check_msgs_.push_back((*mp));
        check_.unlock();
        ntime++;}
    }

    //txs_msgs_ clean
    block_only=true; // allow validation of block messages only
    std::vector<uint64_t> msg_mis;
    winner->get_missing(msg_mis);
    for(auto key : msg_mis){
      auto tm=txs_msgs_.lower_bound(key);
      while(tm!=txs_msgs_.end() && !((tm->second->hash.num ^ (key)) & 0xFFFFFFFFFFFF0000L)){
        if(tm->second->status & MSGSTAT_COM){
          DLOG("FOUND missing message %04X:%08X [len:%d]\n",tm->second->svid,tm->second->msid,tm->second->len);
          //winner->msg_mis.erase(mn);
          winner->del_missing(key);
          break;}
        tm++;}}
    txs_.unlock();
    DLOG("LAST_block_final finished\n");
  }

  void count_votes(uint32_t now,hash_s& cand)
  { extern candidate_ptr nullcnd;
    candidate_ptr cnd1=nullcnd;
    candidate_ptr cnd2=nullcnd;
    uint64_t votes_counted=0;
    hash_s best;
    cand_.lock();
    for(auto it=candidates_.begin();it!=candidates_.end();it++){ // cand_ is locked
      if(cnd1==nullcnd || it->second->score>cnd1->score){
        cnd2=cnd1;
        memcpy(&best,&it->first,sizeof(hash_s));
        cnd1=it->second;}
      else if(cnd2==nullcnd || it->second->score>cnd2->score){
        cnd2=it->second;}
      votes_counted+=it->second->score;}
    cand_.unlock();
    if(cnd1==nullcnd){
      if(do_vote && now>srvs_.now+BLOCKSEC+(do_vote-1)*VOTE_DELAY){
        DLOG("CANDIDATE proposing\n");
        write_candidate(cand);}
      return;}
    if(do_block<2 && (
        (cnd1->score>(cnd2!=nullcnd?cnd2->score:0)+(votes_max-votes_counted))||
        (now>srvs_.now+BLOCKSEC+(BLOCKSEC/2)))){
      uint64_t x=(cnd2!=nullcnd?cnd2->score:0);
      if(now>srvs_.now+BLOCKSEC+(BLOCKSEC/2)){
        ELOG("CANDIDATE SELECTED:%016lX second:%016lX max:%016lX counted:%016lX BECAUSE OF TIMEOUT!!!\n",
          cnd1->score,x,votes_max,votes_counted);}
      else{
        ELOG("CANDIDATE ELECTED:%016lX second:%016lX max:%016lX counted:%016lX\n",
          cnd1->score,x,votes_max,votes_counted);}
      do_block=2;
      winner=cnd1;
      char text[2*SHA256_DIGEST_LENGTH];
      ed25519_key2text(text,best.hash,SHA256_DIGEST_LENGTH);
      ELOG("CAND %.*s elected\n",2*SHA256_DIGEST_LENGTH,text);
      if(winner->failed){
        ELOG("BAD CANDIDATE elected :-(\n");}
      DLOG("STOPing validation to finish msg list\n");
      do_validate=0;
      threadpool.join_all();
      busy_msgs_.clear();
      DLOG("STOPed validation to finish msg list\n");
      LAST_block_final(best);
      if(!winner->elected_accept()){
        do_validate=1;
        for(int v=0;v<VALIDATORS;v++){
          threadpool.create_thread(boost::bind(&server::validator, this));}}}
    if(do_block==2 && winner->elected_accept()){
      ELOG("CANDIDATE winner accepted\n");
      do_block=3;
      if(do_vote){
        write_candidate(best);}
      return;}
    if(do_block==2){
      if(now>srvs_.now+BLOCKSEC+(BLOCKSEC/2)||now>srvs_.now+BLOCKSEC+(VIP_MAX*VOTE_DELAY)){
        ELOG("MISSING MESSAGES, PANIC:\n%s",print_missing_verbose());}
      else{
        DLOG("ELECTION: %s\n",winner->print_missing(&srvs_));
        }}
    if(do_vote && cnd1->accept() && cnd1->peers.size()>1){
      ELOG("CANDIDATE proposal accepted\n");
      write_candidate(best);
      return;}
    if(do_vote && now>srvs_.now+BLOCKSEC+(do_vote-1)*VOTE_DELAY){
      ELOG("CANDIDATE proposing\n");
      write_candidate(cand);}
  }

  void add_electors(header_t& head,svsi_t* peer_svsi)
  { hash_t empty;
    for(int i=0;i<head.vok;i++){
      uint8_t* data=(uint8_t*)&peer_svsi[i];
      uint16_t svid;
      memcpy(&svid,data,2);
      message_ptr msg(new message(MSGTYPE_BLK,(uint8_t*)&head,sizeof(header_t),svid,head.now,NULL,data+2,empty));
      msg->hash.num=msg->dohash(opts_.svid);
      msg->status|=MSGSTAT_VAL;
      msg->svid=svid;
      msg->peer=svid; //to allow insertion
      msg->msid=head.now;
      blk_insert(msg);}
  }
      
  //FIXME, using blk_msgs_ as elector set is unsafe, VIP nodes should be used
  void prepare_poll()
  { 
    cand_.lock();
#ifdef DEBUG
    uint32_t electors_old=electors.size();
#endif
    electors.clear();
    for(auto it=candidates_.begin();it!=candidates_.end();){
      auto jt=it++;
      if(jt->second->now<srvs_.now){
        candidates_.erase(jt);}}
    votes_max=0.0;
    do_vote=0;
    cand_.unlock();
    //FIXME, this should be moved to servers.hpp
    std::set<uint16_t> svid_rset;
    std::vector<uint16_t> svid_rank;
    blk_.lock();
    for(auto it=blk_msgs_.begin();it!=blk_msgs_.end();++it){
      if(last_srvs_.nodes.size()<=it->second->svid){ //maybe not needed
        continue;}
      if(last_srvs_.nodes[it->second->svid].status & SERVER_DBL ||
          known_dbl(it->second->svid)){ // ignore also suspected DBL servers
        DLOG("ELECTOR blk ignore %04X (DBL)\n",it->second->svid);
        continue;}
      if(it->second->msid!=srvs_.now-BLOCKSEC){
        DLOG("ELECTOR blk ignore %04X (time %08X<>%08X)\n",it->second->svid,it->second->msid,srvs_.now-BLOCKSEC);
        continue;}
      if(!(it->second->status & MSGSTAT_VAL)){
        DLOG("ELECTOR blk ignore %04X (invalid)\n",it->second->svid);
        continue;}
      DLOG("ELECTOR accepted:%04X (blk)\n",(it->second->svid));
      svid_rset.insert(it->second->svid);}
    blk_.unlock();
    if(!svid_rset.size()){
      ELOG("ERROR, no valid server for this block :-(\n");}
    else{
      for(auto sv : svid_rset){
        svid_rank.push_back(sv);}
      std::sort(svid_rank.begin(),svid_rank.end(),[this](const uint16_t& i,const uint16_t& j){return(this->last_srvs_.nodes[i].weight>this->last_srvs_.nodes[j].weight);});} //fuck, lambda :-/
    //TODO, save this list
    cand_.lock();
    for(uint32_t j=0;j<VIP_MAX && j<svid_rank.size();j++){
      if(svid_rank[j]==opts_.svid){
        do_vote=1+j;}
      //uint64_t score=last_srvs_.nodes[svid_rank[j]].weight/2+TOTALMASS/(2*(VIP_MAX+1));
      uint64_t score=(last_srvs_.nodes[svid_rank[j]].weight+TOTALMASS)/(2*(VIP_MAX+1));
      ELOG("ELECTOR[%d]=%016lX\n",svid_rank[j],score);
      electors[svid_rank[j]]=score;
      votes_max+=score;}
    extern candidate_ptr nullcnd;
    winner=nullcnd;
    ELOG("ELECTOR max:%016lX\n",votes_max);
#ifdef DEBUG
    if(electors.size()<electors_old && electors.size()<srvs_.vtot/2){
      ELOG("LOST ELECTOR (%d->%d), exiting\n",electors_old,(int)electors.size());
      exit(-1);}
#endif
    cand_.unlock();
  }

  message_ptr message_svidmsid(uint16_t svid,uint32_t msid)
  { extern message_ptr nullmsg;
    union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=0; // hash
    h.dat[1]=0; // message type
    memcpy(h.dat+2,&msid,4);
    memcpy(h.dat+6,&svid,2);
    DLOG("HASH find:%016lX (%04X:%08X) %d:%d\n",h.num,svid,msid,svid,msid);
    txs_.lock();
    message_ptr me=nullmsg;
    auto mi=txs_msgs_.lower_bound(h.num);
    while(mi!=txs_msgs_.end() && mi->second->svid==svid && mi->second->msid==msid){
      if(mi->second->status & MSGSTAT_COM){
        txs_.unlock();
        return mi->second;}
      if((mi->second->status & MSGSTAT_DAT) || (me==nullmsg)){
        me=mi->second;}
      mi++;}
    txs_.unlock();
    return(me);
  }

  const char* print_missing_verbose()
  { extern message_ptr nullmsg;
    static std::string line;
    std::vector<uint64_t> missing;
    winner->get_missing(missing);
    line="";
    for(auto key : missing){
      char miss[64];
      uint32_t msid=(key>>16) & 0xFFFFFFFFL;
      uint16_t svid=(key>>48);
      sprintf(miss,"%04X:%08X",svid,msid);
      line+=miss;
      message_ptr me=message_svidmsid(svid,msid);
      if(me==nullmsg){
        line+=" unknown :-(\n";}
      else{
        line+=" KNOW";
        for(auto sv : me->know){
          sprintf(miss,",%04X",sv);
          line+=miss;}
        line+=" BUSY";
        for(auto sv : me->busy){
          sprintf(miss,",%04X",sv);
          line+=miss;}
        line+=" SENT";
        for(auto sv : me->sent){
          sprintf(miss,",%04X",sv);
          line+=miss;}
        line+="\n";}}
    return(line.c_str());
  }

  message_ptr message_find(message_ptr msg,uint16_t svid) //cnd_/blk_/dbl_/txs_.lock()
  { extern message_ptr nullmsg;
    DLOG("HASH find:%016lX (%04X%08X) %d:%d\n",msg->hash.num,msg->svid,msg->msid,msg->svid,msg->msid);
    assert(msg->data!=NULL);
    if(msg->data[0]==MSGTYPE_GET){
      txs_.lock();
      message_map::iterator it=txs_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=txs_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64 && msg->hash.dat[0]==it->second->hashval(svid)){ //data[4+(svid%64)]
          txs_.unlock();
          return it->second;}
        it++;}
      txs_.unlock();
      return nullmsg;}
    if(msg->data[0]==MSGTYPE_CNG){
      cnd_.lock();
      message_map::iterator it=cnd_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=cnd_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64 && msg->hash.dat[0]==it->second->hashval(svid)){ //data[4+(svid%64)]
          cnd_.unlock();
          return it->second;}
        it++;}
      cnd_.unlock();
#ifdef DEBUG
      DLOG("HASH find failed, CND db:\n");
      for(auto me=cnd_msgs_.begin();me!=cnd_msgs_.end();me++){
        DLOG("HASH have: %016lX (%02X)\n",me->first,me->second->hashval(svid));} //data[4+(svid%64)]
#endif
      return nullmsg;}
    if(msg->data[0]==MSGTYPE_BLG){
      blk_.lock();
      message_map::iterator it=blk_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=blk_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64 && msg->hash.dat[0]==it->second->hashval(svid)){ //data[4+(svid%64)]
          blk_.unlock();
          return it->second;}
        it++;}
      blk_.unlock();
      return nullmsg;}
    if(msg->data[0]==MSGTYPE_DBG){
      dbl_.lock();
      message_map::iterator it=dbl_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=dbl_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64){ //could check MSGSTAT_DAT
          dbl_.unlock();
          return it->second;}
        it++;}
      dbl_.unlock();
      return nullmsg;}
    DLOG("UNKNOWN hashtype:%d %02X\n",(uint32_t)msg->data[0],(uint32_t)msg->data[0]);
    return nullmsg;
  }

  void double_spend(message_ptr msg)
  { //DLOG("WARNING, double spend maybe not yet fully implemented\n");
    if(last_srvs_.nodes[msg->svid].msid>=msg->msid){ //check again correct message timing, probably not needed
      DLOG("IGONRING old double spend message %04X:%08X\n",msg->svid,msg->msid);
      return;}
    dbls_.lock();
    dbl_srvs_.insert(msg->svid);
    dbls_.unlock();
    update(msg);
  }

  void create_double_spend_proof(message_ptr msg1,message_ptr msg2)
  { extern message_ptr nullmsg;
    assert(msg1->svid==msg2->svid);
    assert(msg1->msid==msg2->msid);
    assert(memcmp(msg1->sigh,msg2->sigh,32));
    assert(!do_sync); // should never happen, should never get same msid from same server in a msg_list
    if(msg2->svid==opts_.svid){
      ELOG("FATAL, created own double spend !!!\n");
      exit(-1);}
    if(known_dbl(msg2->svid)){
      DLOG("DROP dbl spend message creation for DBL server (%04X)\n",msg1->svid);
      return;}
    if(msg1->msid<=last_srvs_.nodes[msg1->svid].msid){ //ignore too old messages
      DLOG("DROP dbl spend message creation from old message (%04X:%08X)\n",msg1->svid,msg1->msid);
      return;}
    msg1->load(0xffff); // could use opts_.svid instead of 0xffff (can not use 0!)
    msg2->load(0xffff); // could use opts_.svid instead of 0xffff (can not use 0!)
    assert(msg1->data[0]==msg2->data[0]);
    hash_t msha={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    if(msg2->data[0]==MSGTYPE_MSG){
      if(msg2->msid==1 || msg2->msid-1==last_srvs_.nodes[msg2->svid].msid){
        memcpy(msha,last_srvs_.nodes[msg2->svid].msha,32);}
      else{
        message_ptr pre=message_svidmsid(msg2->svid,msg2->msid-1);
        if(pre==nullmsg || !(pre->status & (MSGSTAT_DAT|MSGSTAT_VAL))){
          ELOG("ERROR loading message %04X:%08X before double spend\n",msg2->svid,msg2->msid-1);
          msg1->unload(0xffff);
          msg2->unload(0xffff);
          return;}
        else{
          memcpy(msha,pre->sigh,32);}}}
    uint32_t len=4+32+msg1->len+msg2->len;
    message_ptr dbl_msg(new message(len));
    dbl_msg->data[0]=MSGTYPE_DBL;
    memcpy(dbl_msg->data+1,&len,3);
    memcpy(dbl_msg->data+4,msha,32);
    memcpy(dbl_msg->data+4+32,msg1->data,msg1->len);
    memcpy(dbl_msg->data+4+32+msg1->len,msg2->data,msg2->len);
    dbl_msg->msid=msg1->msid; //now, report correct msid of double spend messages
    //dbl_msg->msid=0xffffffff; // any double spend message in this block will have this value
    dbl_msg->svid=msg1->svid;
    dbl_msg->now=time(NULL);
    dbl_msg->peer=opts_.svid;
    dbl_msg->hash.num=dbl_msg->dohash();
    //memcpy(dbl_msg->sigh,msg1->sigh,32); //store sigh from first received message 
    dbl_msg->null_signature(); //this way update will only send 1 version per msid on the network
    msg1->unload(0xffff);
    msg2->unload(0xffff);
    dbl_.lock();
    dbl_msgs_[dbl_msg->hash.num]=dbl_msg;
    dbl_.unlock();
    double_spend(dbl_msg);
    dbl_msg->path=srvs_.now;
    dbl_msg->save(); // just for the record
    //maybe we should also unload it
  }

  bool known_dbl(uint16_t svid)
  { dbls_.lock();
    if(dbl_srvs_.find(svid)!=dbl_srvs_.end()){
      dbls_.unlock();
      return(true);}
    dbls_.unlock();
    return(false);
  }

  int check_dbl(boost::mutex& mlock_,message_map& msgs,message_map::iterator it)
  { extern message_ptr nullmsg;
    message_ptr pre=nullmsg,nxt=nullmsg,msg=it->second; //probably not needed when syncing
    assert(msg->data!=NULL);
    assert(it!=msgs.end());
    mlock_.lock();
    if(it!=msgs.begin()){
      pre=(--it)->second;
      it++;}
    if((++it)!=msgs.end()){
      nxt=it->second;}
    it--;
    mlock_.unlock();
    assert(pre!=it->second);
    assert(nxt!=it->second);
    if(pre!=nullmsg && (pre->hash.num&0xFFFFFFFFFFFF0000L)==(msg->hash.num&0xFFFFFFFFFFFF0000L)){
      DLOG("HASH insert:%016lX [len:%d] DOUBLE SPEND (%016lX) [len:%d] !\n",msg->hash.num,msg->len,pre->hash.num,pre->len);
      if(pre->len>message::header_length && msg->len>message::header_length){
        bad_insert(msg);
        create_double_spend_proof(pre,msg);
        return(-1);} // double spend
      return(1);} // possible double spend
    if(nxt!=nullmsg && (nxt->hash.num&0xFFFFFFFFFFFF0000L)==(msg->hash.num&0xFFFFFFFFFFFF0000L)){
      DLOG("HASH insert:%016lX [len:%d] DOUBLE SPEND (%016lX) [len:%d] !\n",msg->hash.num,msg->len,nxt->hash.num,nxt->len);
      if(nxt->len>message::header_length && msg->len>message::header_length){
        bad_insert(msg);
        create_double_spend_proof(nxt,msg);
        return(-1);} // double spend
      return(1);} // possible double spend
    return(0);
  }

  void bad_insert(message_ptr msg)
  { assert(!(msg->status&MSGSTAT_BAD));
    bad_.lock();
    hash_s* hash_p=(hash_s*)msg->sigh;
    auto it=bad_msgs_.find(*hash_p);
    //auto it=bad_msgs_.find(*(hash_s*)msg->sigh);
    if(it==bad_msgs_.end()){
      char text[2*32];
      ed25519_key2text(text,msg->sigh,32);
      DLOG("BAD message %04X:%08X %016lX %.64s\n",msg->svid,msg->msid,msg->hash.num,text);
      if(msg->status&MSGSTAT_SAV){
        msg->bad_insert();}
      else{
        msg->status|=MSGSTAT_BAD;
        msg->save();}
      hash_s* hash_p2=(hash_s*)msg->sigh;
      bad_msgs_[*hash_p2]=msg;}
      //bad_msgs_[*(hash_s*)msg->sigh]=msg;
    else{
      assert(!(msg->status&MSGSTAT_SAV));
      msg->status|=MSGSTAT_BAD;
      if(!(msg->status & MSGSTAT_SIG) && (it->second->status & MSGSTAT_SIG)){
        it->second->status &= ~MSGSTAT_SIG;}}
    bad_.unlock();
  }

  void bad_recover(message_ptr msg)
  { bad_.lock();
    hash_s* hash_p=(hash_s*)msg->sigh;
    bad_msgs_.erase(*hash_p);
    //bad_msgs_.erase(*(hash_s*)msg->sigh);
    if(msg->status&MSGSTAT_SAV){
      msg->bad_recover();}
    else{
      msg->status &= ~MSGSTAT_BAD;
      if(!msg->path){
        msg->path=LAST_block;}
      msg->save();}
    bad_.unlock();
  }

  int message_insert(message_ptr msg)
  { if(msg->hash.dat[1]==MSGTYPE_MSG){
      return(txs_insert(msg));}
    if(msg->hash.dat[1]==MSGTYPE_CND){
      return(cnd_insert(msg));}
    if(msg->hash.dat[1]==MSGTYPE_BLK){
      return(blk_insert(msg));}
    if(msg->hash.dat[1]==MSGTYPE_DBL){
      return(dbl_insert(msg));}
    ELOG("ERROR, getting unexpected message type :-( \n");
    return(-1);
  }

  int dbl_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  {
    assert(msg->hash.dat[1]==MSGTYPE_DBL);
    dbl_.lock(); // maybe no lock needed
    DLOG("HASH insert:%016lX (DBL)\n",msg->hash.num);
    auto it=dbl_msgs_.find(msg->hash.num);
    if(it!=dbl_msgs_.end()){
      message_ptr osg=it->second;
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
        if(last_srvs_.nodes[msg->svid].msid>=msg->msid){
          dbl_.unlock();
          DLOG("IGONRING old double spend message %04X:%08X\n",msg->svid,msg->msid);
          missing_msgs_erase(msg);
          return(0);}
        osg->update(msg);
        osg->path=srvs_.now;
        if(!osg->save()){
          ELOG("ERROR, message save failed, abort server\n");
          exit(-1);}
        dbl_.unlock();
        missing_msgs_erase(msg);
        if(!known_dbl(osg->svid)){
          double_spend(osg);}
        osg->unload(0);
        return(1);}
      else{ // update info about peer inventory
        dbl_.unlock();
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    if(msg->len==message::header_length){
      dbl_msgs_[msg->hash.num]=msg;
      dbl_.unlock();
      missing_msgs_insert(msg);
      return(1);}
    dbl_.unlock();
    DLOG("ERROR, getting unexpected dbl message\n");
    return(-1);
  }

  int cnd_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  { assert(msg->hash.dat[1]==MSGTYPE_CND);
    cnd_.lock();
    DLOG("HASH insert:%016lX (CND) [len:%d]\n",msg->hash.num,msg->len);
    message_map::iterator it=cnd_msgs_.find(msg->hash.num);
    if(it!=cnd_msgs_.end()){
      message_ptr osg=it->second;
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
	//message_ptr pre=nullmsg,nxt=nullmsg;
        osg->update(msg);
        osg->path=osg->msid; // this is the block time!!!
        cnd_.unlock();
        if(osg->msid>LAST_block){
          //TODO, this should not happen
          DLOG("ERROR, cnd message %04X:%08X to early :-(, keep in missing_msgs_\n",osg->svid,osg->msid);
          return(0);}
        missing_msgs_erase(msg);
        if(check_dbl(cnd_,cnd_msgs_,it)<0){ // removes message from map
          DLOG("ERROR, cnd message %04X:%08X maybe double spend\n",osg->svid,osg->msid);
          if(known_dbl(msg->svid)){ return(1);}
          return(-1);}
        DLOG("DEBUG, store cnd message %04X:%08X\n",osg->svid,osg->msid);
        cnd_validate(osg);
        return(1);}
      else{ // update info about peer inventory
        cnd_.unlock();
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    if(msg->len==message::header_length){
      std::pair<message_map::iterator,bool> ret;
      ret=cnd_msgs_.insert(std::pair<uint64_t,message_ptr>(msg->hash.num,msg));
      cnd_.unlock();
      missing_msgs_insert(msg);
      if(check_dbl(cnd_,cnd_msgs_,ret.first)){
        if(known_dbl(msg->svid)){ return(1);}
        return(-1);}
      return(1);}
    if(msg->svid==opts_.svid){ // own message
      cnd_msgs_[msg->hash.num]=msg;
      msg->path=msg->msid; // this is the block time!!!
      assert(msg->peer==msg->svid);
      DLOG("DEBUG, storing own cnd message\n");
      //msg->save(); //too sort to waste disk space
      cnd_.unlock();
      cnd_validate(msg);
      return(1);}
    cnd_.unlock();
    DLOG("ERROR, getting unexpected cnd message\n");
    return(-1);
  }

  int blk_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  { assert(msg->hash.dat[1]==MSGTYPE_BLK);
    blk_.lock();
    DLOG("HASH insert:%016lX (BLK) (len:%d)\n",msg->hash.num,msg->len);
    message_map::iterator it=blk_msgs_.find(msg->hash.num);
    if(it!=blk_msgs_.end()){
      message_ptr osg=it->second;
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
	//message_ptr pre=nullmsg,nxt=nullmsg;
        osg->update(msg);
        osg->path=osg->msid; // this is the block time!!!
        blk_.unlock();
        missing_msgs_erase(msg);
        if(check_dbl(blk_,blk_msgs_,it)<0){ // removes message from map
          if(known_dbl(msg->svid)){ return(1);}
          return(-1);}
        DLOG("DEBUG, storing blk message from %04X\n",osg->svid);
        blk_validate(osg);
        return(1);}
      else{ // update info about peer inventory
        blk_.unlock();
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    if(msg->len==message::header_length){
      std::pair<message_map::iterator,bool> ret;
      ret=blk_msgs_.insert(std::pair<uint64_t,message_ptr>(msg->hash.num,msg));
      blk_.unlock();
      missing_msgs_insert(msg);
      if(check_dbl(blk_,blk_msgs_,ret.first)){
        if(known_dbl(msg->svid)){ return(1);}
        return(-1);}
      return(1);}
    if(msg->svid==opts_.svid){ // own message
      blk_msgs_[msg->hash.num]=msg;
      msg->path=msg->msid; // this is the block time!!!
      assert(msg->peer==msg->svid);
      DLOG("DEBUG, storing own blk message\n");
      //msg->save(); //too short to waste disk space
      blk_.unlock();
      blk_validate(msg);
      return(1);}
    if(msg->svid==msg->peer){ // peers message
      blk_msgs_[msg->hash.num]=msg;
      msg->path=msg->msid; // this is the block time!!!
      DLOG("DEBUG, storing peer's %04X blk message\n",msg->svid);
      //msg->save(); //too short to waste disk space
      blk_.unlock();
      blk_validate(msg);
      return(1);}
    blk_.unlock();
    DLOG("ERROR, getting unexpected blk message\n");
    return(-1);
  }

  int txs_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  { assert(msg->hash.dat[1]==MSGTYPE_MSG);
    if(last_srvs_.nodes[msg->svid].status & SERVER_DBL){ // BKY can change current status (srvs_.nodes[].status)
      DLOG("HASH insert:%016lX (TXS) [len:%d] DBLserver ignored\n",msg->hash.num,msg->len);
      return(0);}
    txs_.lock(); // maybe no lock needed
    //DLOG("HASH insert:%016lX (TXS) [len:%d]\n",msg->hash.num,msg->len);
    message_map::iterator it=txs_msgs_.find(msg->hash.num);
    if(it!=txs_msgs_.end()){
      message_ptr osg=it->second;
      // overwrite message status with the current one
      // not needed any more msg->status=osg->status; // for peer.hpp to check if message is already validated
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
        //if(do_sync && memcmp(osg->sigh,msg->sigh,SHA256_DIGEST_LENGTH))
        if((osg->status & MSGSTAT_VAL) && memcmp(osg->sigh,msg->sigh,SHA256_DIGEST_LENGTH)){
          txs_.unlock();
          DLOG("HASH insert:%016lX (TXS) [len:%d] WRONG SIGNATURE HASH!\n",msg->hash.num,msg->len);
//FIXME, save as failed
          bad_insert(msg);
          return(0);}
        osg->update(msg);
        osg->path=srvs_.now;
        txs_.unlock();
        missing_msgs_erase(msg);
        if(!do_sync){
          if(check_dbl(txs_,txs_msgs_,it)<0){ // saves messages as bad if double spend
            if(known_dbl(msg->svid)){ return(1);}
            return(-1);}}
        if(!osg->save()){ //FIXME, change path
          DLOG("HASH insert:%016lX (TXS) [len:%d] SAVE FAILED, ABORT!\n",osg->hash.num,osg->len);
          exit(-1);}
        osg->unload(0);
        if(osg->now>=srvs_.now+BLOCKSEC){
          DLOG("HASH insert:%016lX (TXS) [len:%d] delay to %08X/ ???\n",osg->hash.num,osg->len,osg->now);
          wait_.lock();
          wait_msgs_.push_back(osg);
          wait_.unlock();
          return(1);}//FIXME, process wait messages later
        DLOG("HASH insert:%016lX (TXS) [len:%d] queued\n",osg->hash.num,osg->len);
        check_.lock();
        check_msgs_.push_back(osg);
        check_.unlock();
        return(1);}
      else{ // update info about peer inventory
        txs_.unlock();
        DLOG("HASH insert:%016lX (TXS) [len:%d] ignored\n",msg->hash.num,msg->len);
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    else{
      if(msg->len==message::header_length){
        //txs_msgs_[msg->hash.num]=msg;
        std::pair<message_map::iterator,bool> ret;
        ret=txs_msgs_.insert(std::pair<uint64_t,message_ptr>(msg->hash.num,msg));
        txs_.unlock();
        DLOG("HASH insert:%016lX (TXS) [len:%d] set as missing\n",msg->hash.num,msg->len);
        missing_msgs_insert(msg);
        if(!do_sync && check_dbl(txs_,txs_msgs_,ret.first)){
          if(known_dbl(msg->svid)){ return(1);}
          return(-1);}
        return(1);}
      if(msg->svid==opts_.svid){ // own message
        txs_msgs_[msg->hash.num]=msg;
	//if(msg->path && msg->path!=srvs_.now){ //move message
        //  DLOG("HASH insert:%016lX (TXS) [len:%d] path mismatch (%08X<>%08X)\n",
        //    msg->hash.num,msg->len,msg->path,srvs_.now);
        //  exit(-1);}
        //assert(msg->path==srvs_.now || msg->path==srvs_.now+BLOCKSEC);
        msg->path=srvs_.now; // all messages must be in current block
        DLOG("HASH insert:%016lX (TXS) [len:%d] store as own\n",msg->hash.num,msg->len);
	//saved before insertion
        if(!msg->save()){
          ELOG("ERROR, failed to save own message %08X, fatal\n",msg->msid);
          exit(-1);}
        txs_.unlock();
        msg->unload(0);
        if(!(msg->status & MSGSTAT_BAD)){ // only during DOUBLE_SPEND tests
          if(msg->now>=srvs_.now+BLOCKSEC){
            DLOG("HASH insert:%016lX (TXS) [len:%d] delay to %08X/ own message\n", //FIXME, fatal in start !!!
              msg->hash.num,msg->len,srvs_.now+BLOCKSEC);
            wait_.lock();
            wait_msgs_.push_back(msg);
            wait_.unlock();}
          else{
            check_.lock();
            check_msgs_.push_back(msg); // running though validator
            check_.unlock();}}
	assert(msg->peer==msg->svid);
        return(1);}
      txs_.unlock();
      ELOG("HASH insert:%016lX (TXS) [len:%d] UNEXPECTED!\n",msg->hash.num,msg->len);
      return(-1);}
  }

  void cnd_validate(message_ptr msg) //FIXME, check timing !!!
  { if(msg->msid<srvs_.now){
      DLOG("ERROR dropping old candidate message %04X:%08X\n",msg->svid,msg->msid);
      return;}
    DLOG("CANDIDATE test\n");
    cand_.lock();
    if(electors.find(msg->svid)==electors.end()){ //FIXME, electors should have assigned rank when building poll
      DLOG("BAD ELECTOR %04X\n",msg->svid);
      cand_.unlock();
      //msg->status|=MSGSTAT_BAD;
      return;}
    if(!electors[msg->svid]){
      cand_.unlock();
      //msg->status|=MSGSTAT_BAD;
      return;}
    hash_s cand; //TODO
    assert(msg->data!=NULL);
    memcpy(cand.hash,msg->data+message::data_offset,sizeof(hash_t));
    auto it=candidates_.find(cand);
    if(it==candidates_.end()){
      DLOG("BAD CANDIDATE :-( \n");
      cand_.unlock();
      //msg->status|=MSGSTAT_BAD;
      return;}
    msg->status|=MSGSTAT_VAL;
    it->second->score+=electors[msg->svid]; // update sum of weighted votes
    DLOG("CANDIDATE score:%016lX (added:%016lX)\n",it->second->score,electors[msg->svid]);
    electors[msg->svid]=0;
    cand_.unlock();
    update(msg); // update others
  }

  void blk_validate(message_ptr msg) // WARNING, this is executed by peer io_service
  {

//FIXME, check what block are we dealing with !!!
//FIXME, if message too early, add anyway

    assert(msg->path==msg->msid);
    DLOG("BLOCK test\n");
    if(msg->msid!=last_srvs_.now){ //FIXME, this fails during authentication
      DLOG("BLOCK bad msid:%08x block:%08x\n",msg->msid,last_srvs_.now);
      return;}
    uint32_t vip=last_srvs_.nodes[msg->svid].status & SERVER_VIP;
    if(!vip){
      DLOG("BLOCK ignore non-vip vote msid:%08x svid:%04x\n",msg->msid,(uint32_t)msg->svid);
      return;}
    msg->status|=MSGSTAT_VAL;
    assert(msg->data!=NULL);
    header_t* h=(header_t*)(msg->data+4+64+10); 
    bool no=memcmp(h->nowhash,last_srvs_.nowhash,sizeof(hash_t));
    //blk_.lock();
    last_srvs_.save_signature(msg->svid,msg->data+4,!no);
    //blk_.unlock();
    DLOG("BLOCK: yes:%d no:%d max:%d\n",last_srvs_.vok,last_srvs_.vno,last_srvs_.vtot);
    update(msg); // update others if this is a VIP message, my message was sent already, but second check will not harm
    if(last_srvs_.vno>last_srvs_.vtot/2){
      ELOG("BAD BLOCK consensus :-( must resync :-( \n"); // FIXME, do not exit, initiate sync
      exit(-1);}
    if(no){
      if(msg->peer==msg->svid){
        //FIXME, in the future, do not disconnect, peer will try syncing again
        ELOG("\n\nBLOCK differs, disconnect from %04X! if connected\n\n\n",msg->svid);
        disconnect(msg->svid);}}
  }

  void missing_sent_remove(uint16_t svid) //TODO change name to missing_know_send_remove()
  { missing_.lock();
    for(auto mi=missing_msgs_.begin();mi!=missing_msgs_.end();mi++){
      //mi->second->sent_erase(svid);
      mi->second->know_sent_erase(svid);}
    missing_.unlock();
  }

  void validator(void)
  { message_ptr busy_msg;
    while(do_validate){
      check_.lock(); //TODO this should be a lock for check_msgs_ only maybe
      busy_msgs_.erase(busy_msg);
      if(check_msgs_.empty()){
        check_.unlock();
        uint32_t now=time(NULL);
	message_queue tmp_msgs_;
	missing_.lock();
	for(auto mj=missing_msgs_.begin();mj!=missing_msgs_.end();){
          //FIXME, request _BLK messages if BLOCK ready 
          auto mi=mj++;
          if(mi->second->hash.dat[1]==MSGTYPE_BLK || mi->second->hash.dat[1]==MSGTYPE_CND){ //FIXME, consider checking mi->first
            if(mi->second->msid<last_srvs_.now-BLOCKSEC){
              DLOG("DEBUG, ERASING MISSING MESSAGE %04X:%08X\n",mi->second->svid,mi->second->msid);
              missing_msgs_.erase(mi);
              continue;}
            if(now>mi->second->got+MAX_MSGWAIT && (
              (mi->second->hash.dat[1]==MSGTYPE_BLK && mi->second->msid<srvs_.now) ||
              (mi->second->hash.dat[1]==MSGTYPE_CND && mi->second->msid<=srvs_.now))){
              tmp_msgs_.push_back(mi->second);}
            continue;}
          if(block_only){
            if(mi->second->status & MSGSTAT_VAL && srvs_.nodes[mi->second->svid].msid==mi->second->msid-1){
              tmp_msgs_.push_back(mi->second);}
            continue;}
          if(mi->second->msid<srvs_.nodes[mi->second->svid].msid){
            missing_msgs_.erase(mi);
            continue;}
          if(mi->second->got<=now-MAX_MSGWAIT && srvs_.nodes[mi->second->svid].msid==mi->second->msid-1){
            tmp_msgs_.push_back(mi->second);}}
	missing_.unlock();
        for(auto re=tmp_msgs_.begin();re!=tmp_msgs_.end();re++){
	  uint16_t svid=(*re)->request();
          if(svid){
            //assert((*re)->data!=NULL);
            DLOG("HASH request:%016lX (%04X) %d:%d\n",(*re)->hash.num,svid,(*re)->svid,(*re)->msid);
            DLOG("REQUESTING MESSAGE %04X:%08X from %04X\n",(*re)->svid,(*re)->msid,svid);
            deliver((*re),svid);}}
        //checking waiting messages
        tmp_msgs_.clear();
        wait_.lock();
	for(auto wa=wait_msgs_.begin();wa!=wait_msgs_.end();){
          if(block_only){
            if(((*wa)->status & MSGSTAT_VAL) && srvs_.nodes[(*wa)->svid].msid==(*wa)->msid-1){
              DLOG("QUEUING MESSAGE %04X:%08X\n",(*wa)->svid,(*wa)->msid);
              tmp_msgs_.push_back(*wa);
              wa=wait_msgs_.erase(wa);}
            else{
              wa++;}
            continue;}
          if((*wa)->now<srvs_.now+BLOCKSEC && srvs_.nodes[(*wa)->svid].msid==(*wa)->msid-1){
            DLOG("QUEUING MESSAGE %04X:%08X\n",(*wa)->svid,(*wa)->msid);
            tmp_msgs_.push_back(*wa);
            wa=wait_msgs_.erase(wa);}
          else{
            wa++;}}
        wait_.unlock();
	check_.lock();
        check_msgs_.insert(check_msgs_.end(),tmp_msgs_.begin(),tmp_msgs_.end());
        //prevent adding duplicates // no need for this any more because we check busy_msgs_
        //for(auto tm : tmp_msgs_){
        //  for(auto cm : check_msgs_){
        //    if(tm==cm){
        //      goto NEXT;}}
        //  check_msgs_.push_back(tm);
        //  NEXT:;} 
        //TODO, check if there are no forgotten messeges in the missing_msgs_ queue
        if(check_msgs_.empty()){
          check_.unlock();
	  boost::this_thread::sleep(boost::posix_time::milliseconds(50));
          RETURN_ON_SHUTDOWN();}
        else{
          check_.unlock();}}
      else{
        busy_msg=check_msgs_.front();
	check_msgs_.pop_front();
        auto notbusy=busy_msgs_.emplace(busy_msg);
        //could concider a custom lock check against opening the same usr/XXXX.dat file
        //this will be needed later if we need to provide usr/XXXX.dat file for syncing
        check_.unlock();
        if(!notbusy.second){
          DLOG("WARNING ignoring validation of busy message %04X:%08X\n",busy_msg->svid,busy_msg->msid);
          continue;}
        if(busy_msg->status & MSGSTAT_BAD){
          DLOG("WARNING ignoring validation of bad message %04X:%08X\n",busy_msg->svid,busy_msg->msid);
          continue;}
        if(busy_msg->status & MSGSTAT_COM){
          DLOG("WARNING ignoring validation of committed message %04X:%08X\n",busy_msg->svid,busy_msg->msid);
          continue;}
        if(!(busy_msg->status & MSGSTAT_VAL) && dbl_srvs_.find(busy_msg->svid)!=dbl_srvs_.end()){ // ignore from DBL server
          DLOG("WARNING ignoring validation of invalid message %04X:%08X from DBL nodes (double?:%s)\n",
            busy_msg->svid,busy_msg->msid,(last_srvs_.nodes[busy_msg->svid].status&SERVER_DBL?"yes":"no"));
          continue;} //
        //if(busy_msg->status==MSGSTAT_VAL || srvs_.nodes[busy_msg->svid].msid>=busy_msg->msid)
        if(srvs_.nodes[busy_msg->svid].msid>=busy_msg->msid){
          DLOG("WARNING ignoring validation of old message %04X:%08X (<=%08X)\n",
            busy_msg->svid,busy_msg->msid,srvs_.nodes[busy_msg->svid].msid);
          continue;}
	if(srvs_.nodes[busy_msg->svid].msid!=busy_msg->msid-1){ //assume only 1 validator per bank
          DLOG("WARNING postponing validation of future message %04X:%08X (!=%08X+1)\n",
            busy_msg->svid,busy_msg->msid,srvs_.nodes[busy_msg->svid].msid);
          wait_.lock();
          wait_msgs_.push_back(busy_msg);
          wait_.unlock();
          continue;}
        if(!(busy_msg->status & MSGSTAT_VAL) && block_only){ // ignore from DBL server
          DLOG("WARNING postponing validation of invalid message %04X:%08X during block finish\n",
            busy_msg->svid,busy_msg->msid);
          wait_.lock();
          wait_msgs_.push_back(busy_msg);
          wait_.unlock();
          continue;}
        if(!busy_msg->load(0)){
          ELOG("ERROR, failed to load blk/%03X/%05X/%02x_%04x_%08x.msg [len:%d]\n",busy_msg->path>>20,busy_msg->path&0xFFFFF,(uint32_t)busy_msg->hashtype(),busy_msg->svid,busy_msg->msid,busy_msg->len);
          exit(-1);}
        bool valid=process_message(busy_msg); //maybe ERROR should be also returned.
        if(valid){
          busy_msg->print_text("COMMITED");
          busy_msg->status|=MSGSTAT_COM;
          node* nod=&srvs_.nodes[busy_msg->svid];
          nod->msid=busy_msg->msid;
          nod->mtim=busy_msg->now;
          memcpy(nod->msha,busy_msg->sigh,sizeof(hash_t));
          if(busy_msg->svid==opts_.svid && msid_<nod->msid){
            ELOG("WARNING !!! increasing local msid by network !!!\n");
            msid_=nod->msid;}
          uint32_t now=time(NULL);
          uint64_t next=busy_msg->hash.num+1;
	  missing_.lock();
          auto nt=missing_msgs_.lower_bound(next); //speed up next validation
          if(nt!=missing_msgs_.end()){
            if(nt->second->got>now-MAX_MSGWAIT){
              nt->second->got=now-MAX_MSGWAIT;}}
	  missing_.unlock();}
        else{
          //FIXME, if own message, try fixing :-(
          //TODO, inform peer if peer==author;
          ELOG("ERROR, have invalid message %04X:%08X !!!\n",busy_msg->svid,busy_msg->msid);
          if(busy_msg->status & MSGSTAT_VAL){
            ELOG("ERROR, failed to validate valid message %04X:%08X, fatal\n",busy_msg->svid,busy_msg->msid);
            exit(-1);} //FIXME, die gently
          //if(!(busy_msg->status & MSGSTAT_BAD)){
          //  busy_msg->remove();} //save under new name (as failed)
          bad_insert(busy_msg);
#ifdef DEBUG
          exit(-1); //only for debugging
#endif
          continue;}
        if(busy_msg->path<srvs_.now){
          DLOG("MOVING message %04X:%08X to %08X/ after validation\n",busy_msg->svid,busy_msg->msid,srvs_.now);
          busy_msg->move(srvs_.now);}
        busy_msg->unload(0);
        if(!do_sync){
          //simulate delay, FIXME, remove after sync tests
#ifdef DEBUG
          uint32_t seconds=(rand()%5);
          DLOG("SLEEP %d after validation\n",seconds);
          boost::this_thread::sleep(boost::posix_time::seconds(seconds));
          RETURN_ON_SHUTDOWN();
#endif
          update_candidates(busy_msg);
          update(busy_msg);}
        else{
          ldc_.lock();
          ldc_msgs_.erase(busy_msg->hash.num);
          ldc_.unlock();}}}
  }

  //uint64_t make_ppi(uint32_t amsid,uint16_t abank,uint16_t bbank)
  uint64_t make_ppi(uint16_t tmpos, uint32_t omsid,uint32_t amsid,uint16_t abank,uint16_t bbank)
  { ppi_t ppi;
    ppi.v16[0]=tmpos;
    ppi.v16[1]=(uint16_t)(0xFFFF & (amsid-omsid));
    ppi.v16[2]=abank;
    ppi.v16[3]=bbank;
    return(ppi.v64);
  }

  uint16_t ppi_abank(const uint64_t& ppi)
  { ppi_t *p=(ppi_t*)&ppi;
    return(p->v16[2]);
  }

  uint16_t ppi_bbank(const uint64_t& ppi)
  { ppi_t *p=(ppi_t*)&ppi;
    return(p->v16[3]);
  }

  uint64_t ppi_txid(const uint64_t& ppi)
  { ppi_t *p=(ppi_t*)&ppi;
    p->v16[3]=p->v16[0];
    p->v32[0]=last_srvs_.nodes[p->v16[3]].msid+p->v16[1];
    return(p->v64); // msid,svid,tpos
  }

  bool remove_message(message_ptr msg)
  { if(msg->svid==opts_.svid){
#ifdef DOUBLE_SPEND
      if(opts_.svid == 4){ // make mess :-)
        msid_=srvs_.nodes[opts_.svid].msid;
        DLOG("IGNORING message problems on double spend server, set msid_=%08X\n",msid_);
        return(false);}
#endif
      ELOG("ERROR: trying to remove own message, MUST RESUBMIT (TODO!)\n");
      exit(-1);}
    return(true);
  }
  /*bool remove_message(message_ptr msg) // log removing of message
  { uint8_t* p=(uint8_t*)msg->data+4+64+10;
    std::map<uint64_t,log_t> log;
    //TODO, load message from file
    uint32_t now=time(NULL);
    while(p<(uint8_t*)msg->data+msg->len){
      //uint8_t txstype=*p;
      usertxs utxs;
      assert(*p<TXSTYPE_INF);
      utxs.parse((char*)p);
      if(*p==TXSTYPE_CON){
        p+=utxs.size;
        continue;}
      uint32_t mpos=(p-msg->data);
      if(utxs.abank==opts_.svid){
        uint64_t key=((uint64_t)utxs.auser)<<32;
        key|=mpos;
        log_t alog;
        alog.time=now;
        alog.type=*p|0x4000; //outgoing|removed
        alog.node=utxs.bbank;
        alog.user=utxs.buser;
        alog.umid=utxs.amsid;
        alog.nmid=msg->msid; //can be overwritten with info
        alog.mpos=mpos; //can be overwritten with info
        alog.weight=utxs.tmass;
        alog.info !!!
        log[key]=alog;}
      if((*p==TXSTYPE_PUT || *p==TXSTYPE_GET) && utxs.bbank==opts_.svid){
        uint64_t key=((uint64_t)utxs.buser)<<32;
        key|=mpos;
        log_t blog;
        blog.time=now;
        blog.type=*p|0x8000|0x4000; //incoming|removed
        blog.node=utxs.abank;
        blog.user=utxs.auser;
        blog.umid=utxs.amsid;
        blog.nmid=msg->msid; //can be overwritten with info
        blog.mpos=mpos; //can be overwritten with info
        blog.weight=utxs.tmass;
        blog.info !!!
        log[key]=blog;}
      if(*p==TXSTYPE_MPT){
        char* tbuf=utxs.toaddresses((char*)p);
        for(int i=0;i<utxs.bbank;i++,tbuf+=6+8){
          uint16_t tbank;
          memcpy(&tbank,tbuf+0,2);
          if(tbank==opts_.svid){
            uint32_t tuser;
             int64_t tmass;
            memcpy(&tuser,tbuf+2,4);
            memcpy(&tmass,tbuf+6,8);
            uint64_t key=((uint64_t)tuser)<<32;
            key|=mpos;
            log_t blog;
            blog.time=now;
            blog.type=*p|0x8000|0x4000; //incoming|removed
            blog.node=utxs.abank;
            blog.user=utxs.auser;
            blog.umid=utxs.amsid;
            blog.nmid=msg->msid; //can be overwritten with info
            blog.mpos=mpos; //can be overwritten with info
            blog.weight=tmass;
            blog.info !!!
            log[key]=blog;}}}
      p+=utxs.size;}
    put_log(opts_.svid,log); //TODO, add loging options for multiple banks
    return(true);
  }*/

  bool undo_message(message_ptr msg) //FIXME, this is single threaded, remove locks
  { if(!msg->load(0)){
      ELOG("ERROR, failed to load message !!!\n");
      exit(-1);}
    assert(msg->data!=NULL);
    assert(msg->status & MSGSTAT_COM);
    char* p=(char*)msg->data+4+64+10;
    std::map<uint64_t,int64_t> txs_deposit;
    //std::set<uint64_t> txs_get; //set lock / withdraw
    std::set<uint16_t> old_bky;
    uint32_t tmpos=0;
    uint32_t omsid=last_srvs_.nodes[msg->svid].msid; //must exists
    while(p<(char*)msg->data+msg->len){
      tmpos++;
      usertxs utxs;
      assert(*p<TXSTYPE_INF); //FIXME, use a different check
      if(*p==TXSTYPE_PUT){
        utxs.parse(p);
        if(utxs.bbank!=utxs.abank){
#ifdef DEBIG
          DLOG("WARNING undoing put\n");
#endif
          union {uint64_t big;uint32_t small[2];} to;
          to.small[0]=utxs.buser; //assume big endian
          to.small[1]=utxs.bbank; //assume big endian
          txs_deposit[to.big]+=utxs.tmass;} // will be substructed at the end of undo
	p+=utxs.size;
	continue;}
      if(*p==TXSTYPE_MPT){
        utxs.parse(p);
        char* tbuf=utxs.toaddresses(p);
        for(int i=0;i<utxs.bbank;i++,tbuf+=6+8){
          uint16_t tbank;
          uint32_t tuser;
           int64_t tmass;
          memcpy(&tbank,tbuf+0,2);
          memcpy(&tuser,tbuf+2,4);
          memcpy(&tmass,tbuf+6,8);
          if(tbank!=utxs.abank){
#ifdef DEBUG
            DLOG("WARNING undoing mpt to: %04X:%08X<=%016lX\n",tbank,tuser,tmass);
#endif
            union {uint64_t big;uint32_t small[2];} to;
            to.small[0]=tuser; //assume big endian
            to.small[1]=tbank; //assume big endian
            txs_deposit[to.big]+=tmass;}} // will be substructed at the end of undo
        p+=utxs.size;
        continue;}
      if(*p==TXSTYPE_BKY){ //reverse bank key change
        utxs.parse(p);
        uint16_t node=msg->svid;
        if(utxs.bbank){
          if((srvs_.nodes[msg->svid].status & SERVER_UNO) && (last_srvs_.nodes[utxs.bbank].status & SERVER_DBL)){
            node=utxs.bbank;}
          else{
            node=0;}}
        if(node && old_bky.find(node)==old_bky.end()){
          memcpy(srvs_.nodes[node].pk,utxs.opkey(p),32);
          old_bky.insert(node);
          if(node==opts_.svid){
            if(utxs.bbank){
              ofip_change_pkey((uint8_t*)utxs.opkey(p));}
            DLOG("WARNING undoing local bank key change\n");}}
        if(node){
          uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,node);
          blk_.lock();
          blk_bky.erase(ppb);
          blk_.unlock();}
        p+=utxs.size;
        DLOG("WARNING undoing bky\n");
        continue;}
      if(*p==TXSTYPE_BNK){
        utxs.parse(p);
        uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,msg->svid);
        blk_.lock();
        blk_bnk.erase(ppb);
        blk_.unlock();
        p+=utxs.size;
        DLOG("WARNING undoing bnk\n");
        continue;}
      if(*p==TXSTYPE_GET){
        utxs.parse(p);
        uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,utxs.bbank);
        blk_.lock();
        blk_get.erase(ppb);
        blk_.unlock();
        p+=utxs.size;
        DLOG("WARNING undoing get\n");
        continue;}
      if(*p==TXSTYPE_USR){
        utxs.parse(p);
        uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,msg->svid);
        blk_.lock();
        blk_usr.erase(ppb);
        blk_.unlock();
        p+=utxs.size;
        DLOG("WARNING undoing usr\n");
        continue;}
      if(*p==TXSTYPE_UOK){
        utxs.parse(p);
        uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,msg->svid);
        blk_.lock();
        blk_uok.erase(ppb);
        blk_.unlock();
        p+=utxs.size;
        DLOG("WARNING undoing uok\n");
        continue;}
      if(*p==TXSTYPE_SBS){
        utxs.parse(p);
        uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,utxs.bbank);
        blk_.lock();
        blk_sbs.erase(ppb);
        blk_.unlock();
        p+=utxs.size;
        DLOG("WARNING undoing sbs\n");
        continue;}
      if(*p==TXSTYPE_UBS){
        utxs.parse(p);
        uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,utxs.bbank);
        blk_.lock();
        blk_ubs.erase(ppb);
        blk_.unlock();
        p+=utxs.size;
        DLOG("WARNING undoing ubs\n");
        continue;}
      p+=utxs.get_size(p);}
//  uint64_t ppi=make_ppi(msg->msid,msg->svid,msg->svid);
//  blk_.lock();
//  blk_usr.erase(ppi);
//  blk_uok.erase(ppi);
//  blk_bnk.erase(ppi);
//  blk_bky.erase(ppi);
    //blk_sbs.erase(ppi);
    //blk_ubs.erase(ppi);
    //for(auto it=txs_get.begin();it!=txs_get.end();it++){
    //  blk_get.erase(*it);}
//  blk_.unlock();
    deposit_.lock();
    for(auto it=txs_deposit.begin();it!=txs_deposit.end();it++){
      deposit[it->first]-=it->second;}
    deposit_.unlock();
    std::map<uint32_t,user_t> undo;
     int64_t weight;
     int64_t fee;
    uint64_t csum[4];
    uint8_t msha[SHA256_DIGEST_LENGTH];
    uint32_t mtim;
    uint32_t users=msg->load_undo(undo,csum,weight,fee,msha,mtim);
    srvs_.nodes[msg->svid].weight-=weight;
    bank_fee[msg->svid]-=fee;
    //FIXME, check write_message timing conflict if this is our own message
    srvs_.xor4(srvs_.nodes[msg->svid].hash,csum);
    srvs_.nodes[msg->svid].msid=msg->msid-1; //LESZEK ADDED assuming message exists
    memcpy(srvs_.nodes[msg->svid].msha,msha,SHA256_DIGEST_LENGTH);
    srvs_.nodes[msg->svid].mtim=mtim;
    //this could be a srvs_.function()
    DLOG("UNDO USERS:%08X\n",users);
    if(users){
      if(srvs_.nodes[msg->svid].users!=users){
        DLOG("WARNING undoing user additions (users back to:%08X)\n",users);}
      srvs_.nodes[msg->svid].users=users;}
    int fd=open_bank(msg->svid);
    //char filename[64];
    //sprintf(filename,"usr/%04X.dat",msg->svid);
    //int fd=open(filename,O_RDWR|O_CREAT,0644);
    //if(fd<0){
    //  ELOG("ERROR, failed to open bank register %04X, fatal\n",msg->svid);
    //  exit(-1);}
    for(auto it=undo.begin();it!=undo.end();it++){
      //user_t& u=it->second;
      DLOG("UNDO:%04X:%08X m:%08X t:%08X s:%04X b:%04X u:%08X l:%08X r:%08X v:%016lX\n",
          msg->svid,it->first,it->second.msid,it->second.time,it->second.stat,it->second.node,
          it->second.user,it->second.lpath,it->second.rpath,it->second.weight);
      lseek(fd,it->first*sizeof(user_t),SEEK_SET);
      write(fd,&it->second,sizeof(user_t));}
    close(fd);
    del_msglog(srvs_.now,msg->svid,msg->msid);
    msg->unload(0);
    msg->status &= ~MSGSTAT_COM;
    return(true);
  }

  void log_broadcast(uint32_t path,char* p,int len,uint8_t* hash,uint8_t* pkey,uint32_t msid,uint32_t mpos)
  { static uint32_t lpath=0;
    static int fd=-1;
    static boost::mutex local_;
    boost::lock_guard<boost::mutex> lock(local_);
    if(path!=lpath || fd<0){
      if(fd>=0){
        close(fd);}
      lpath=path;
      char filename[64];
      sprintf(filename,"blk/%03X/%05X/bro.log",path>>20,path&0xFFFFF);
      fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644); //TODO maybe O_TRUNC not needed
      if(fd<0){
        DLOG("ERROR, failed to open BROADCAST LOG %s\n",filename);
        return;}}
    write(fd,p,len);
    write(fd,hash,32);
    write(fd,pkey,32);
    write(fd,&msid,sizeof(uint32_t));
    write(fd,&mpos,sizeof(uint32_t)); //FIXME, make this uint16_t
  }

  bool process_message(message_ptr msg)
  { assert(msg->data!=NULL);
    if(msg->now<last_srvs_.now){
      DLOG("ERROR MSG %04X:%08X too old :-( (%08X<%08X)\n",msg->svid,msg->msid,msg->now,last_srvs_.now);
      return(false);}
    char text[2*32];
    ed25519_key2text(text,msg->sigh,32);
    bool check_sig=((!(msg->status & MSGSTAT_VAL) && !do_sync && msg->svid!=opts_.svid)?true:false);
    uint32_t tpos_max=*(uint32_t*)(msg->data+msg->len+32+4+4);
    assert(tpos_max);
    if(!check_sig){
      tpos_max=1;}
    DLOG("PROCESS MSG %04X:%08X [tp:%d] %.64s\n",msg->svid,msg->msid,tpos_max,text);
    int sign_num=0;
    //remember pthreads stack size limit 2Mb
    //int sign_valid[tpos_max];
    //size_t sign_mlen[tpos_max];
    //size_t sign_mlen2[tpos_max];
    //hash_s sign_pk_hash[tpos_max]; //2MB :-( exceeds stack size :-(
    //hash_s sign_m__hash[tpos_max]; //2MB :-( exceeds stack size :-(
    //const unsigned char* sign_m[tpos_max];
    //const unsigned char* sign_m2[tpos_max];
    //const unsigned char* sign_pk[tpos_max];
    //const unsigned char* sign_rs[tpos_max];
    std::vector<int> sign_valid(tpos_max);
    std::vector<size_t> sign_mlen(tpos_max);
    std::vector<size_t> sign_mlen2(tpos_max);
    std::vector<hash_s> sign_pk_hash(tpos_max);
    std::vector<hash_s> sign_m__hash(tpos_max);
    std::vector<const unsigned char*> sign_m(tpos_max);
    std::vector<const unsigned char*> sign_m2(tpos_max);
    std::vector<const unsigned char*> sign_pk(tpos_max);
    std::vector<const unsigned char*> sign_rs(tpos_max);
    char* p=(char*)msg->data+4+64+10;
    int fd=open_bank(msg->svid);
    std::map<uint64_t,log_t> log;
    std::map<uint32_t,user_t> changes;
    std::map<uint32_t,user_t> undo;
    //std::map<uint32_t,uint64_t> local_deposit;
    std::map<uint32_t,dsu_t> local_dsu;
    std::map<uint64_t,int64_t> txs_deposit;
    std::map<uint64_t,hash_s> txs_bky;
    std::map<uint64_t,uint32_t> txs_bnk; //create new bank
    std::map<uint64_t,get_t> txs_get; //set lock / withdraw
    std::map<uint64_t,usr_t> txs_usr; //remote account request
    std::map<uint64_t,uok_t> txs_uok; //remote account accept
    std::map<uint64_t,uint32_t> txs_sbs; //set node status bits
    std::map<uint64_t,uint32_t> txs_ubs; //clear node status bits
    uint32_t users=srvs_.nodes[msg->svid].users;
    uint32_t ousers=users;
    uint32_t lpath=srvs_.now;
    uint32_t now=time(NULL); //needed for the log
    uint32_t lpos=1; // needed for log
    int mpt_size=0;
    std::vector<uint16_t> mpt_bank; // for MPT to local bank
    std::vector<uint32_t> mpt_user; // for MPT to local bank
    std::vector< int64_t> mpt_mass; // for MPT to local bank
    uint64_t csum[4]={0,0,0,0};
     int64_t weight=0; //FIXME fix weight calculation later !!! (include correct fee handling)
    //uint64_t ppi=make_ppi(msg->msid,msg->svid,msg->svid);
    int64_t local_fee=0;
    int64_t lodiv_fee=0;
    int64_t myput_fee=0; // remote bank_fee for local bank
    //uint32_t tnum=0;
    uint32_t tmpos=0;
    uint32_t omsid=last_srvs_.nodes[msg->svid].msid; //must exists
    for(;p<(char*)msg->data+msg->len;){ //tnum++
      uint32_t luser=0;
      uint16_t lnode=0;
      int64_t deduct=0;
      int64_t fee=0;
      int64_t remote_fee=0;
      tmpos++;
//FIXME, save this at message end
      //uint32_t mpos=((uint8_t*)p-(uint8_t*)msg->data); // should not be used;
      /************* START PROCESSING **************/
      user_t* usera=NULL;
      usertxs utxs;
      if(!utxs.parse(p)){
        DLOG("ERROR: failed to parse transaction\n");
        close(fd);
        return(false);}
#ifdef DEBUG
      utxs.print_head();
#endif
      if(*p==TXSTYPE_NON){
        p+=utxs.size;
	//?? no fee :-(
        continue;}
      if(*p==TXSTYPE_CON){
        srvs_.nodes[msg->svid].port=utxs.abank;
        srvs_.nodes[msg->svid].ipv4=utxs.auser;
        p+=utxs.size;
	//?? no fee :-(
        continue;}
      if(*p>=TXSTYPE_INF){
        ELOG("ERROR: unknown transaction\n");
        close(fd);
        return(false);}
      if(utxs.ttime>lpath+BLOCKSEC+5){ // remember that values are unsigned !
	ELOG("ERROR: time in the future block time:%08X block:%08X limit %08X\n",
	  utxs.ttime,lpath,lpath+BLOCKSEC+5);
        close(fd);
        return(false);}
      if(utxs.abank!=msg->svid){
        ELOG("ERROR: bad bank\n");
        close(fd);
        return(false);}
      if((*p==TXSTYPE_USR && utxs.abank==utxs.bbank) || *p==TXSTYPE_UOK){ // check lock first
        char* lpkey;
        if(*p==TXSTYPE_USR){
	  luser=utxs.nuser(p);
          lpkey=utxs.npkey(p);}
        else{ //UOK
	  luser=utxs.auser;
          lpkey=utxs.upkey(p);}
        if(luser>users){
          DLOG("ERROR: bad target user id %08X\n",luser);
          close(fd);
          return(false);}
	if(luser<users){ //1. check if overwriting was legal
          auto lu=changes.find(luser); // get user
          if(lu==changes.end()){
            user_t u;
            lseek(fd,luser*sizeof(user_t),SEEK_SET); // should return '0s' for new user, ok for xor4
            read(fd,&u,sizeof(user_t));
            changes[luser]=u;
            undo[luser]=u;
            usera=&changes[luser];}
          else{ // there should be no previous transaction on this user !!!
            usera=&lu->second;}
          int64_t delta=usera->weight;
          if(!(usera->stat&USER_STAT_DELETED)){
            ELOG("ERROR, overwriting active account %04X:%08X [weight:%016lX]\n",
              utxs.bbank,luser,usera->weight);
            close(fd);
            return(false);}
          local_fee+=delta;
          weight-=delta;}
        else{
          user_t u;
          bzero(&u,sizeof(user_t));
          users++;
          changes[luser]=u;
          usera=&changes[luser];}
	srvs_.xor4(csum,usera->csum);
        if(*p==TXSTYPE_USR){
          srvs_.init_user(*usera,msg->svid,luser,USER_MIN_MASS,(uint8_t*)lpkey,utxs.ttime,utxs.abank,utxs.auser);}
        else{
          srvs_.init_user(*usera,msg->svid,luser,0,(uint8_t*)lpkey,utxs.ttime,utxs.bbank,utxs.buser);}
	srvs_.xor4(csum,usera->csum);
        srvs_.put_user(*usera,msg->svid,luser);
        if(*p==TXSTYPE_USR){
	  weight+=USER_MIN_MASS;}
        else{ //*p==TXSTYPE_UOK
          uok_t uok;
          uok.auser=utxs.auser;
          uok.bbank=utxs.bbank;
          uok.buser=utxs.buser;
          memcpy(uok.pkey,lpkey,32);
          uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,msg->svid); //not utxs.bbank
          txs_uok[ppb]=uok;
          p+=utxs.size;
          continue;}}
      if(utxs.auser>=users){
        ELOG("ERROR, bad userid %04X:%08X\n",utxs.abank,utxs.auser);
        close(fd);
        return(false);}
      auto au=changes.find(utxs.auser); // get user
      if(au==changes.end()){
        user_t u;
        lseek(fd,utxs.auser*sizeof(user_t),SEEK_SET); // should return '0s' for new user, ok for xor4
        read(fd,&u,sizeof(user_t));
        changes[utxs.auser]=u;
        undo[utxs.auser]=u;
        usera=&changes[utxs.auser];}
      else{
        usera=&au->second;}
      //do not check signature for valid messages
      if(check_sig){
        assert(sign_num<(int)tpos_max);
        int mlen2=utxs.sign_mlen2();
        assert(p+mlen2<(char*)msg->data+msg->len);
        sign_mlen[sign_num]=(size_t)32;
        sign_mlen2[sign_num]=(size_t)mlen2;
        memcpy(sign_m__hash[sign_num].hash,usera->hash,32);
        sign_m[sign_num]=sign_m__hash[sign_num].hash;
        memcpy(sign_pk_hash[sign_num].hash,usera->pkey,32);
        sign_pk[sign_num]=sign_pk_hash[sign_num].hash;
        sign_m2[sign_num]=(unsigned char*)p;
        sign_rs[sign_num]=(unsigned char*)p+mlen2;
        sign_num++;}
      ////during tests do not perform a second check of the signatures from office
      //if((!(msg->status & MSGSTAT_VAL) || (iamvip && !do_sync))
      //    && msg->svid!=opts_.svid //FIXME, remove this after tests !!!
      //    && utxs.wrong_sig((uint8_t*)p,(uint8_t*)usera->hash,(uint8_t*)usera->pkey)){
      //  //TODO postpone this and run this as batch verification
      //  ELOG("ERROR: bad signature\n");
      //  close(fd);
      //  return(false);}
      if(usera->msid!=utxs.amsid){
        ELOG("ERROR: bad msid %04X:%08X\n",usera->msid,utxs.amsid);
        close(fd);
        return(false);}
      //process transactions
      if(usera->time+LOCK_TIME<lpath && usera->user && usera->node && (usera->user!=utxs.auser || usera->node!=utxs.abank)){//check account lock
        if(*p!=TXSTYPE_PUT || utxs.abank!=utxs.bbank || utxs.auser!=utxs.buser || utxs.tmass!=0){
          DLOG("ERROR: account locked, send 0 to yourself and wait for unlock\n");
          close(fd);
          return(false);}}
      else if(*p==TXSTYPE_BRO){
        //log_broadcast(lpath,p,utxs.size,usera->hash,usera->pkey,msg->msid,mpos);
        log_broadcast(lpath,p,utxs.size,usera->hash,usera->pkey,msg->msid,tmpos);
        //utxs.print_broadcast(p);
        fee=TXS_BRO_FEE(utxs.bbank);}
      else if(*p==TXSTYPE_PUT){
        if(utxs.tmass<0){ //sending info about negative values is allowed to fascilitate exchanges
          utxs.tmass=0;}
        //if(utxs.abank!=utxs.bbank && utxs.auser!=utxs.buser && !check_user(utxs.bbank,utxs.buser))
        if(!srvs_.check_user(utxs.bbank,utxs.buser)){
          // does not check if account closed [consider adding this slow check]
          ELOG("ERROR: bad target user %04X:%08X\n",utxs.bbank,utxs.buser);
          close(fd);
          return(false);}
        if(utxs.bbank==utxs.abank){
          //local_deposit[utxs.buser]+=utxs.tmass;
          local_dsu[utxs.buser].deposit+=utxs.tmass;}
        else{
          union {uint64_t big;uint32_t small[2];} to;
          to.small[0]=utxs.buser; //assume big endian
          to.small[1]=utxs.bbank; //assume big endian
          txs_deposit[to.big]+=utxs.tmass;}
        deduct=utxs.tmass;
        fee=TXS_PUT_FEE(utxs.tmass);
        if(utxs.abank!=utxs.bbank){
          if(utxs.bbank==opts_.svid){
            myput_fee+=TXS_LNG_FEE(utxs.tmass);}
          remote_fee+=TXS_LNG_FEE(utxs.tmass);
          fee+=TXS_LNG_FEE(utxs.tmass);}}
      else if(*p==TXSTYPE_MPT){
        char* tbuf=utxs.toaddresses(p);
        utxs.tmass=0;
        mpt_size=0;
        mpt_bank.reserve(utxs.bbank);
        mpt_user.reserve(utxs.bbank);
        mpt_mass.reserve(utxs.bbank);
        std::set<uint64_t> out;
        union {uint64_t big;uint32_t small[2];} to;
        to.small[1]=0;
        fee=TXS_MIN_FEE;
        for(int i=0;i<utxs.bbank;i++,tbuf+=6+8){
          uint32_t& tuser=to.small[0];
          uint32_t& tbank=to.small[1];
          //uint16_t tbank;
          //uint32_t tuser;
           int64_t tmass;
          memcpy(&tbank,tbuf+0,2);
          memcpy(&tuser,tbuf+2,4);
          memcpy(&tmass,tbuf+6,8);
          if(tmass<=0){ //only positive non-zero values allowed
            DLOG("ERROR: only positive non-zero transactions allowed in MPT\n");
            close(fd);
            return(false);}
          if(out.find(to.big)!=out.end()){
            ELOG("ERROR: duplicate target: %04X:%08X\n",tbank,tuser);
            close(fd);
            return(false);}
          if(!srvs_.check_user((uint16_t)tbank,tuser)){
            ELOG("ERROR: bad target user %04X:%08X\n",utxs.bbank,utxs.buser);
            close(fd);
            return(false);}
          out.insert(to.big);
          if((uint16_t)tbank==utxs.abank){
            //local_deposit[tuser]+=tmass;
            local_dsu[tuser].deposit+=tmass;}
          else{
            //union {uint64_t big;uint32_t small[2];} to;
            //to.small[0]=tuser; //assume big endian
            //to.small[1]=tbank; //assume big endian
            txs_deposit[to.big]+=tmass;}
          if((uint16_t)tbank==opts_.svid){
            mpt_bank[mpt_size]=tbank;
            mpt_user[mpt_size]=tuser;
            mpt_mass[mpt_size]=tmass;
            mpt_size++;}
          fee+=TXS_MPT_FEE(tmass);
          if(utxs.abank!=tbank){
            if((uint16_t)tbank==opts_.svid){
              myput_fee+=TXS_LNG_FEE(tmass);}
            remote_fee+=TXS_LNG_FEE(tmass);
            fee+=TXS_LNG_FEE(tmass);}
          utxs.tmass+=tmass;}
        deduct=utxs.tmass;}
      else if(*p==TXSTYPE_USR){ // this is local bank
        if(utxs.abank!=utxs.bbank){
          usr_t usr;
          usr.auser=utxs.auser;
          usr.bbank=utxs.bbank;
          memcpy(usr.pkey,usera->pkey,32);
          uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,msg->svid); //not utxs.bbank
          txs_usr[ppb]=usr;
          if(utxs.bbank==opts_.svid){ //respond to account creation request
            ofip_add_remote_user(utxs.abank,utxs.auser,usera->pkey);}}
        deduct=USER_MIN_MASS;
        if(utxs.abank!=utxs.bbank){
          fee=TXS_USR_FEE;}
        else{
          fee=TXS_MIN_FEE;}}
      else if(*p==TXSTYPE_BNK){ // we will get a confirmation from the network
        uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,msg->svid); //not utxs.bbank
        txs_bnk[ppb]=utxs.auser;
        deduct=BANK_MIN_TMASS;
        fee=TXS_BNK_FEE;}
      else if(*p==TXSTYPE_GET){
        if(utxs.abank==utxs.bbank){
          ELOG("ERROR: bad bank %04X, use PUT\n",utxs.bbank);
          close(fd);
          return(false);}
        uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,utxs.bbank);
        get_t get;
        get.auser=utxs.auser;
        get.buser=utxs.buser;
        memcpy(get.pkey,usera->pkey,32);
        txs_get[ppb]=get;
        fee=TXS_GET_FEE;}
      else if(*p==TXSTYPE_KEY){
        memcpy(usera->pkey,utxs.key(p),32);
        fee=TXS_KEY_FEE;}
      else if(*p==TXSTYPE_BKY){ // we will get a confirmation from the network
        if(utxs.auser){
          ELOG("ERROR: bad user %08X for node key changes\n",utxs.auser);
          close(fd);
          return(false);}
        if(utxs.bbank>=last_srvs_.nodes.size()){
          ELOG("ERROR: bad node %04X for node key changes\n",utxs.bbank);
          close(fd);
          return(false);}
        uint16_t node=msg->svid;
        if(utxs.bbank){
          if((srvs_.nodes[msg->svid].status & SERVER_UNO) && (last_srvs_.nodes[utxs.bbank].status & SERVER_DBL)){
            node=utxs.bbank;}
          else{
            node=0;}}
        if(node){
          if(memcmp(srvs_.nodes[node].pk,utxs.opkey(p),32)){
            ELOG("ERROR: bad current node key for %04X in %04X:%08X\n",node,msg->svid,msg->msid);
            close(fd);
            return(false);}
          else{
            if(node==opts_.svid){
              ofip_change_pkey((uint8_t*)utxs.key(p));
              ELOG("WARNING, changing my node key !\n");}
            uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,node);
            txs_bky[ppb]=*(hash_s*)utxs.key(p);}}
        fee=TXS_BKY_FEE;}
      else if(*p==TXSTYPE_SBS){
        if(utxs.auser){
          ELOG("ERROR: bad user %04X for node status changes\n",utxs.auser);
          close(fd);
          return(false);}
        if(utxs.bbank>=last_srvs_.nodes.size()){
          ELOG("ERROR: bad node %04X for node key changes\n",utxs.bbank);
          close(fd);
          return(false);}
        if((last_srvs_.nodes[msg->svid].status & SERVER_VIP) || (utxs.abank==utxs.bbank)){ // only VIP nodes vote
          uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,utxs.bbank);
          txs_sbs[ppb]=(uint32_t)utxs.tmass;}
        fee=TXS_SBS_FEE;}
      else if(*p==TXSTYPE_UBS){
        if(utxs.auser){
          ELOG("ERROR: bad user %04X for node status changes\n",utxs.auser);
          close(fd);
          return(false);}
        if(utxs.bbank>=last_srvs_.nodes.size()){
          ELOG("ERROR: bad node %04X for node key changes\n",utxs.bbank);
          close(fd);
          return(false);}
        if((last_srvs_.nodes[msg->svid].status & SERVER_VIP) || (utxs.abank==utxs.bbank)){ // only VIP nodes vote
          uint64_t ppb=make_ppi(tmpos,omsid,msg->msid,msg->svid,utxs.bbank);
          txs_ubs[ppb]=(uint32_t)utxs.tmass;}
        fee=TXS_UBS_FEE;}
      else if(*p==TXSTYPE_SUS){
        if(!srvs_.check_user(utxs.bbank,utxs.buser)){
          ELOG("ERROR: bad target user %04X:%08X\n",utxs.bbank,utxs.buser);
          close(fd);
          return(false);}
        if(utxs.bbank==utxs.abank){
          uint16_t bits=(uint16_t)utxs.tmass & 0xFFFE; //can not change USER_STAT_DELETED
          uint16_t mask=local_dsu[utxs.buser].uus & bits;
          local_dsu[utxs.buser].uus&=~mask;
          local_dsu[utxs.buser].sus|=bits & ~mask;}
        fee=TXS_SUS_FEE;}
      else if(*p==TXSTYPE_UUS){
        if(!srvs_.check_user(utxs.bbank,utxs.buser)){
          ELOG("ERROR: bad target user %04X:%08X\n",utxs.bbank,utxs.buser);
          close(fd);
          return(false);}
        if(utxs.bbank==utxs.abank){
          uint16_t bits=(uint16_t)utxs.tmass & 0xFFFE; //can not change USER_STAT_DELETED
          uint16_t mask=local_dsu[utxs.buser].sus & bits;
          local_dsu[utxs.buser].sus&=~mask;
          local_dsu[utxs.buser].uus|=bits & ~mask;}
        fee=TXS_UUS_FEE;}
      else if(*p==TXSTYPE_SAV){
        if(!(msg->status & MSGSTAT_VAL) && iamvip && !do_sync && msg->path>=start_path){
          user_t u;
          msg->get_user(utxs.auser,u);
          if(memcmp(&u,utxs.usr(p),sizeof(user_t))){ // can fail if we don't have data from this block :-(
            ELOG("ERROR: bad user data for %04X:%08X\n",utxs.abank,utxs.auser);
            close(fd);
            return(false);}}
        fee=TXS_SAV_FEE;}
      int64_t div=dividend(*usera,lodiv_fee); //do this before checking balance
      if(div!=(int64_t)0x8FFFFFFFFFFFFFFF){
        //DLOG("DIV: pay to %04X:%08X (%016lX)\n",msg->svid,utxs.auser,div);
        weight+=div;}
      if(deduct+fee+(utxs.auser?0:BANK_MIN_UMASS)>usera->weight){ //network accepts total withdrawal from user
        ELOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
          deduct,fee,(uint64_t)(utxs.auser?0:BANK_MIN_UMASS),usera->weight);
        close(fd);
        return(false);}
      if(msg->svid!=opts_.svid){
        if((*p==TXSTYPE_PUT || *p==TXSTYPE_SUS || *p==TXSTYPE_UUS || *p==TXSTYPE_GET) && utxs.bbank==opts_.svid){
          uint64_t key=(uint64_t)utxs.buser<<32;
          key|=lpos++;
          log_t blog;
          blog.time=now;
          blog.type=*p|0x8000; //incoming
          blog.node=utxs.abank;
          blog.user=utxs.auser;
          blog.umid=utxs.amsid;
          blog.nmid=msg->msid; //can be overwritten with info
          //blog.mpos=mpos; //can be overwritten with info
          blog.mpos=tmpos; //can be overwritten with info
          blog.weight=utxs.tmass;
          if(*p==TXSTYPE_PUT){
            memcpy(blog.info,utxs.tinfo,32);}
          else{ //TXSTYPE_GET
            memcpy(blog.info+ 0,&usera->weight,8);
            memcpy(blog.info+ 8,&deduct,8);
            memcpy(blog.info+16,&fee,8);
            memcpy(blog.info+24,&usera->stat,2);
            memcpy(blog.info+26,&usera->pkey,6);}
          log[key]=blog;}
        if(*p==TXSTYPE_MPT && mpt_size>0){ //only bbank==my in mpt_....[]
          log_t blog;
          blog.time=now;
          blog.type=*p|0x8000; //incoming
          blog.node=utxs.abank;
          blog.user=utxs.auser;
          blog.umid=utxs.amsid;
          blog.nmid=msg->msid; //can be overwritten with info
          //blog.mpos=mpos; //can be overwritten with info
          blog.mpos=tmpos; //can be overwritten with info
          memcpy(blog.info+ 0,&usera->weight,8);
          memcpy(blog.info+ 8,&deduct,8);
          memcpy(blog.info+16,&fee,8);
          memcpy(blog.info+24,&usera->stat,2);
          memcpy(blog.info+26,&usera->pkey,6);
          for(int i=0;i<mpt_size;i++){
            if(mpt_bank[i]==opts_.svid){
              uint64_t key=(uint64_t)mpt_user[i]<<32;
              key|=lpos++;
              blog.weight=mpt_mass[i];
              blog.info[31]=(i?0:1);
              //bzero(blog.info,32);
              log[key]=blog;}}}}
      usera->msid++;
      usera->time=utxs.ttime;
      usera->node=lnode;
      usera->user=luser;
      usera->lpath=lpath;
      //convert message to hash
      uint8_t hash[32];
      SHA256_CTX sha256;
      SHA256_Init(&sha256);
      //SHA256_Update(&sha256,p,txslen[(int)*p]+64);
      SHA256_Update(&sha256,utxs.get_sig(p),64);
      SHA256_Final(hash,&sha256);
      //make newhash=hash(oldhash+newmessagehash);
      SHA256_Init(&sha256);
      SHA256_Update(&sha256,usera->hash,32);
      SHA256_Update(&sha256,hash,32);
      SHA256_Final(usera->hash,&sha256);
      //usera->weight+=local_deposit[utxs.auser]-deduct-fee;
      //weight+=local_deposit[utxs.auser]-deduct-fee;
      //local_deposit[utxs.auser]=0;//to find changes[utxs.auser]
      usera->weight+=local_dsu[utxs.auser].deposit-deduct-fee;
      usera->stat|=local_dsu[utxs.auser].sus;
      usera->stat&=~local_dsu[utxs.auser].uus;
      weight+=local_dsu[utxs.auser].deposit-deduct-fee;
      local_dsu[utxs.auser].deposit=0;//to find changes[utxs.auser]
      local_dsu[utxs.auser].sus=0;//to find changes[utxs.auser]
      local_dsu[utxs.auser].uus=0;//to find changes[utxs.auser]
      local_fee+=fee-remote_fee;
      p+=utxs.size;}
    if(check_sig){
      //std::vector<int> sign_valid(tpos_max);
      //std::vector<size_t> sign_mlen(tpos_max);
      //std::vector<size_t> sign_mlen2(tpos_max);
      //std::vector<hash_s> sign_pk_hash(tpos_max);
      //std::vector<hash_s> sign_m__hash(tpos_max);
      //std::vector<const unsigned char*> sign_m(tpos_max);
      //std::vector<const unsigned char*> sign_m2(tpos_max);
      //std::vector<const unsigned char*> sign_pk(tpos_max);
      //std::vector<const unsigned char*> sign_rs(tpos_max);
      int ret=ed25519_sign_open_batch2(&sign_m[0],&sign_mlen[0],&sign_m2[0],&sign_mlen2[0],&sign_pk[0],&sign_rs[0],sign_num,&sign_valid[0]);
      if(ret){ //TODO create detailed report (report each failed message)
        ELOG("ERROR checking signatures for %04X:%08X\n",msg->svid,msg->msid);
        close(fd);
        return(false);}}
    //commit local changes
    user_t u;
    //const int offset=(char*)&u+sizeof(user_t)-(char*)&u.stat;
    //for(auto it=local_deposit.begin();it!=local_deposit.end();it++)
    for(auto it=local_dsu.begin();it!=local_dsu.end();it++){
      auto jt=changes.find(it->first);
      if(jt!=changes.end()){
        srvs_.xor4(csum,jt->second.csum);
        //jt->second.weight+=it->second;
	//weight+=it->second;
        jt->second.weight+=it->second.deposit;
	weight+=it->second.deposit;
        jt->second.stat|=it->second.sus;
        jt->second.stat&=~it->second.uus;
	srvs_.user_csum(jt->second,msg->svid,it->first);
        srvs_.xor4(csum,jt->second.csum);}
        //lseek(fd,jt->first*sizeof(user_t),SEEK_SET);
        //write(fd,&jt->second,sizeof(user_t));
      else{
        lseek(fd,it->first*sizeof(user_t),SEEK_SET);
        read(fd,&u,sizeof(user_t));
        undo[it->first]=u;
        srvs_.xor4(csum,u.csum);
        int64_t div=dividend(u,lodiv_fee); // store local fee
        if(div!=(int64_t)0x8FFFFFFFFFFFFFFF){
          //DLOG("DIV: pay to %04X:%08X (%016lX)\n",msg->svid,it->first,div);
          weight+=div;}
        //u.weight+=it->second;
	//weight+=it->second;
        u.weight+=it->second.deposit;
	weight+=it->second.deposit;
        u.stat|=it->second.sus; 
        u.stat&=~it->second.uus;
	srvs_.user_csum(u,msg->svid,it->first);
        srvs_.xor4(csum,u.csum);
        changes[it->first]=u;}}
        //lseek(fd,-offset,SEEK_CUR);
        //write(fd,&u.stat,offset);
    //local_deposit.clear();
    local_dsu.clear();

    int64_t profit=BANK_PROFIT(local_fee+lodiv_fee)-MESSAGE_FEE(msg->len);
    bank_fee[msg->svid]+=profit;
    msg->save_undo(undo,ousers,csum,weight,profit,srvs_.nodes[msg->svid].msha,srvs_.nodes[msg->svid].mtim);
    srvs_.nodes[msg->svid].weight+=weight;
    srvs_.xor4(srvs_.nodes[msg->svid].hash,csum);
    srvs_.save_undo(msg->svid,undo,ousers); //databank, will change srvs_.nodes[msg->svid].users

    //save to /usr/ after writing undo file (requires second round of file seeks)
    //TODO, consider crediting here remote deposits to reduce the size of "std::map<> deposit"
    for(auto jt=changes.begin();jt!=changes.end();jt++){
      lseek(fd,jt->first*sizeof(user_t),SEEK_SET);
      write(fd,&jt->second,sizeof(user_t));}
    changes.clear();
    close(fd);

    //log bank fees
    if(msg->svid==opts_.svid){
      local_fee=BANK_PROFIT(local_fee);
      lodiv_fee=BANK_PROFIT(lodiv_fee);
      myput_fee=BANK_PROFIT(myput_fee);
      log_t alog;
      alog.time=now;
      alog.type=TXSTYPE_FEE|0x8000; //incoming
      alog.node=msg->svid;
      alog.user=0;
      alog.umid=0;
      alog.nmid=msg->msid;
      alog.mpos=0;
      alog.weight=profit;
      memcpy(alog.info,&local_fee,sizeof(int64_t));
      memcpy(alog.info+sizeof(int64_t),&lodiv_fee,sizeof(int64_t));
      memcpy(alog.info+2*sizeof(int64_t),&myput_fee,sizeof(int64_t)); //FIXME, useless !!!
      bzero(alog.info+3*sizeof(int64_t),sizeof(int64_t));
      log[0]=alog;}
    else if(myput_fee){
      local_fee=BANK_PROFIT(local_fee);
      lodiv_fee=BANK_PROFIT(lodiv_fee);
      myput_fee=BANK_PROFIT(myput_fee);
      log_t alog;
      alog.time=now;
      alog.type=TXSTYPE_FEE|0x8000; //incoming
      alog.node=msg->svid;
      alog.user=0;
      alog.umid=0;
      alog.nmid=msg->msid;
      alog.mpos=0;
      alog.weight=myput_fee;
      memcpy(alog.info,&local_fee,sizeof(int64_t)); //FIXME, useless !!!
      memcpy(alog.info+sizeof(int64_t),&lodiv_fee,sizeof(int64_t)); //FIXME, useless !!!
      memcpy(alog.info+2*sizeof(int64_t),&myput_fee,sizeof(int64_t)); //FIXME, useless !!!
      bzero(alog.info+3*sizeof(int64_t),sizeof(int64_t));
      log[0]=alog;}


    //commit remote deposits
    deposit_.lock();
    for(auto it=txs_deposit.begin();it!=txs_deposit.end();it++){
      deposit[it->first]+=it->second;}
    //check for maximum deposits.size(), if too large, save old deposits and work on new ones;
    deposit_.unlock();
    txs_deposit.clear();
    //store block transactions
    blk_.lock();
    for(auto it=txs_bky.begin();it!=txs_bky.end();it++){
      uint16_t node=ppi_bbank(it->first);
      if(last_srvs_.nodes[node].status & SERVER_DBL){
        ELOG("WARNING schedule resetting DBL status for node %04X!\n",node);
        //blk_bky[ppi].push_back(it->first);
        blk_bky[it->first]=node;}
      memcpy(srvs_.nodes[node].pk,it->second.hash,32);
      if(node==opts_.svid){
        if(!srvs_.find_key(it->second.hash,skey)){
          ELOG("ERROR, failed to change to new bank key, fatal!\n");
          exit(-1);}
        pkey=srvs_.nodes[node].pk;}}
    blk_bnk.insert(txs_bnk.begin(),txs_bnk.end());
    blk_get.insert(txs_get.begin(),txs_get.end());
    blk_usr.insert(txs_usr.begin(),txs_usr.end());
    blk_uok.insert(txs_uok.begin(),txs_uok.end());
    blk_sbs.insert(txs_sbs.begin(),txs_sbs.end());
    blk_ubs.insert(txs_ubs.begin(),txs_ubs.end());
    blk_.unlock();
    if(!block_only){ 
      srvs_.msg++;
      srvs_.txs+=tmpos;}
    put_msglog(srvs_.now,msg->svid,msg->msid,log);
    DLOG("COMPLETED MSG %04X:%08X [tp:%d]\n",msg->svid,msg->msid,tpos_max);
    return(true);
  }

  int open_bank(uint16_t svid) //
  { char filename[64];
    DLOG("OPEN usr/%04X.dat\n",svid);
    sprintf(filename,"usr/%04X.dat",svid);
    int fd=open(filename,O_RDWR|O_CREAT,0644);
    if(fd<0){
      ELOG("ERROR, failed to open bank register %04X, fatal\n",svid);
      exit(-1);}
    return(fd);
  }

  uint8_t bitcount(std::vector<uint8_t>& bitvotes,uint8_t min)
  { if(bitvotes.size()<=min){
      return(0);}
    uint8_t res=0,count[8]={0,0,0,0,0,0,0,0};
    for(auto b : bitvotes){
      for(int i=0;i<8;i++){
        count[i]+=(b>>i) & 1;}}
    for(int i=0;i<8;i++){
      if(count[i]>min){
        res|=1<<i;}}
    return(res);
  }

  void commit_block(std::set<uint16_t>& update) //assume single thread
  { mydiv_fee=0;
    myusr_fee=0;
    myget_fee=0;
    uint32_t lpos=1;
    std::map<uint64_t,log_t> log;
    hlog hlg(srvs_.now);

    //match remote account transactions
    std::map<uin_t,std::deque<uint64_t>,uin_cmp> uin; //waiting remote account requests
    for(auto it=blk_usr.begin();it!=blk_usr.end();it++){
      uint16_t abank=ppi_abank(it->first);
      auto tx=&it->second;
      uin_t nuin={tx->bbank,abank,tx->auser,
       {tx->pkey[ 0],tx->pkey[ 1],tx->pkey[ 2],tx->pkey[ 3],tx->pkey[ 4],tx->pkey[ 5],tx->pkey[ 6],tx->pkey[ 7],
        tx->pkey[ 8],tx->pkey[ 9],tx->pkey[10],tx->pkey[11],tx->pkey[12],tx->pkey[13],tx->pkey[14],tx->pkey[15],
        tx->pkey[16],tx->pkey[17],tx->pkey[18],tx->pkey[19],tx->pkey[20],tx->pkey[21],tx->pkey[22],tx->pkey[23],
        tx->pkey[24],tx->pkey[25],tx->pkey[26],tx->pkey[27],tx->pkey[28],tx->pkey[29],tx->pkey[30],tx->pkey[31]}};
      DLOG("REMOTE user request %04X %04X %08X\n",tx->bbank,abank,tx->auser);
      uin[nuin].push_back(ppi_txid(it->first));}
    blk_usr.clear();
    for(auto it=blk_uok.begin();it!=blk_uok.end();it++){ //send funds from matched transactions to new account
      uint16_t abank=ppi_abank(it->first);
      auto tx=&it->second;
      uin_t nuin={abank,tx->bbank,tx->buser,
       {tx->pkey[ 0],tx->pkey[ 1],tx->pkey[ 2],tx->pkey[ 3],tx->pkey[ 4],tx->pkey[ 5],tx->pkey[ 6],tx->pkey[ 7],
        tx->pkey[ 8],tx->pkey[ 9],tx->pkey[10],tx->pkey[11],tx->pkey[12],tx->pkey[13],tx->pkey[14],tx->pkey[15],
        tx->pkey[16],tx->pkey[17],tx->pkey[18],tx->pkey[19],tx->pkey[20],tx->pkey[21],tx->pkey[22],tx->pkey[23],
        tx->pkey[24],tx->pkey[25],tx->pkey[26],tx->pkey[27],tx->pkey[28],tx->pkey[29],tx->pkey[30],tx->pkey[31]}};
      if(uin.find(nuin)!=uin.end() && !uin[nuin].empty()){
        union {uint64_t big;uint32_t small[2];} to;
        to.small[0]=tx->auser;
        to.small[1]=abank;
        deposit[to.big]+=USER_MIN_MASS; //will generate additional fee for the new bank
        if(abank==opts_.svid){
          myusr_fee+=BANK_PROFIT(TXS_LNG_FEE(USER_MIN_MASS));}
        if(tx->bbank==opts_.svid){
          uint64_t key=(uint64_t)tx->buser<<32;
          key|=lpos++;
          log_t alog;
          alog.time=time(NULL);
          alog.type=TXSTYPE_UOK|0x8000; //incoming
          alog.node=abank;
          alog.user=tx->auser;
          alog.umid=1; // 1 == matching _USR transaction found
          alog.nmid=0;
          alog.mpos=srvs_.now;
          alog.weight=0;
          memcpy(alog.info,tx->pkey,32);
          log[key]=alog;}
        DLOG("REMOTE user request %04X %04X %08X matched\n",abank,tx->bbank,tx->buser);
        uint64_t usr_txid=uin[nuin].front();
        uin[nuin].pop_front();
        hlg.save_uso(tx->buser,tx->auser,usr_txid,ppi_txid(it->first));}
      else{
        DLOG("REMOTE user request %04X %04X %08X not found\n",abank,tx->bbank,tx->buser);
        if(tx->bbank==opts_.svid){ // no matching _USR transaction found
          uint64_t key=(uint64_t)tx->buser<<32;
          key|=lpos++;
          log_t alog;
          alog.time=time(NULL);
          alog.type=TXSTYPE_UOK|0x8000; //incoming
          alog.node=abank;
          alog.user=tx->auser;
          alog.umid=0; // 0 == no matching _USR transaction found
          alog.nmid=0;
          alog.mpos=srvs_.now;
          alog.weight=0;
          memcpy(alog.info,tx->pkey,32);
          log[key]=alog;}
        hlg.save_uok(tx->bbank,tx->buser,tx->auser,ppi_txid(it->first));}}
    blk_uok.clear();
    for(auto it=uin.begin();it!=uin.end();it++){ //send back funds from unmatched transactions
      while(!it->second.empty()){
        DLOG("REMOTE user request %04X %04X %08X unmatched\n",it->first.bbank,it->first.abank,it->first.auser);
        union {uint64_t big;uint32_t small[2];} to;
        to.small[0]=it->first.auser;
        to.small[1]=it->first.abank;
        deposit[to.big]+=USER_MIN_MASS;
        if(it->first.auser){
          bank_fee[to.small[1]]-=BANK_PROFIT(TXS_LNG_FEE(USER_MIN_MASS));} //else would generate extra fee for bank
        if(it->first.abank==opts_.svid){
          uint64_t key=(uint64_t)it->first.auser<<32;
          key|=lpos++;
          log_t alog;
          alog.time=time(NULL);
          alog.type=TXSTYPE_UOK|0x8000; //incoming
          alog.node=it->first.bbank;
          alog.user=0;
          alog.umid=0;
          alog.nmid=0;
          alog.mpos=srvs_.now;
          alog.weight=USER_MIN_MASS;
          memcpy(alog.info,it->first.pkey,32);
          log[key]=alog;}
        uint64_t usr_txid=it->second.front();
        it->second.pop_front();
        hlg.save_usr(it->first.auser,it->first.bbank,usr_txid);}}

    //reset DBL status
    if(!blk_bky.empty()){
      for(auto it=blk_bky.begin();it!=blk_bky.end();it++){
        if(srvs_.nodes[it->second].status & SERVER_DBL){
          hlg.save_bky(it->second,ppi_txid(it->first));}
        srvs_.nodes[it->second].status &= ~SERVER_DBL;}
      blk_bky.clear();}

    //process status change requests
    if(!blk_sbs.empty() || !blk_ubs.empty()){
      uint16_t labank=0; //no need to set this
      uint16_t lbbank=0;
      std::vector<uint8_t> bitvotes;
      for(auto it=blk_sbs.rbegin();it!=blk_sbs.rend();it++){ //most important nodes last
        uint16_t abank=ppi_abank(it->first);
        uint16_t bbank=ppi_bbank(it->first);
        uint32_t status=it->second;
        hlg.save_sbs(bbank,status,ppi_txid(it->first));
        if(lbbank!=bbank){
          srvs_.nodes[lbbank].status |=
              (uint32_t)(bitcount(bitvotes,(uint8_t)(srvs_.vtot/2)))<<24;
          labank=0;
          bitvotes.clear();}
        if(abank==bbank){
          srvs_.nodes[bbank].status |= status & 0xFFF8;}// can change bits 4-16
        if(last_srvs_.nodes[abank].status & SERVER_VIP){
          srvs_.nodes[bbank].status |= status & 0xFF0000; // can change bits 17-24
          if(abank==labank){
            bitvotes.back() |= (uint8_t) (status>>24);}
          else{
            bitvotes.push_back((uint8_t) (status>>24));}}
        labank=abank;
        lbbank=bbank;}
      if(lbbank){
        srvs_.nodes[lbbank].status |=
            (uint32_t)(bitcount(bitvotes,(uint8_t)(srvs_.vtot/2)))<<24;
        lbbank=0;
        bitvotes.clear();}
      for(auto it=blk_ubs.rbegin();it!=blk_ubs.rend();it++){ //most important nodes last
        uint16_t abank=ppi_abank(it->first);
        uint16_t bbank=ppi_bbank(it->first);
        uint32_t status=it->second;
        hlg.save_ubs(bbank,status,ppi_txid(it->first));
        if(lbbank!=bbank){
          srvs_.nodes[lbbank].status &=
              (uint32_t)(~(bitcount(bitvotes,(uint8_t)(srvs_.vtot/2))))<<24;
          labank=0;
          bitvotes.clear();}
        if(abank==bbank){
          srvs_.nodes[bbank].status &= ~(status & 0xFFF8);}// can change bits 4-16
        if(last_srvs_.nodes[abank].status & SERVER_VIP){
          srvs_.nodes[bbank].status &= ~(status & 0xFF0000); // can change bits 17-24
          if(abank==labank){
            bitvotes.back() |= (uint8_t) (status>>24);}
          else{
            bitvotes.push_back((uint8_t) (status>>24));}}
        labank=abank;
        lbbank=bbank;}
      if(lbbank){
        srvs_.nodes[lbbank].status |=
            (uint32_t)(bitcount(bitvotes,(uint8_t)(srvs_.vtot/2)))<<24;
        lbbank=0;
        bitvotes.clear();}
      blk_sbs.clear();
      blk_ubs.clear();}

    //create new banks
    if(!blk_bnk.empty()){
      std::set<uint64_t> new_bnk; // list of available banks for takeover
      uint16_t peer=0;
      for(auto it=srvs_.nodes.begin()+1;it!=srvs_.nodes.end();it++,peer++){ // start with bank=1
        if(it->mtim+BANK_MIN_MTIME<srvs_.now && it->weight<BANK_MIN_TMASS){
          uint64_t bnk=it->weight<<16;
          bnk|=peer;
          new_bnk.insert(bnk);}} //unused
      for(auto it=blk_bnk.begin();it!=blk_bnk.end();it++){
        uint16_t abank=ppi_abank(it->first);
        int fd=open_bank(abank);
        //update.insert(abank); no update here
        uint32_t auser=it->second;
        user_t u;
        lseek(fd,(auser)*sizeof(user_t),SEEK_SET);
        read(fd,&u,sizeof(user_t));
        uint16_t peer=0;
        // create new bank and new user
        if(!new_bnk.empty()){
          auto bi=new_bnk.begin();
          peer=(*bi)&0xffff;
          new_bnk.erase(bi);
          ELOG("BANK, overwrite %04X\n",peer);
          srvs_.put_node(u,peer,abank,auser);} //save_undo() in put_node() !!!
        else if(srvs_.nodes.size()<BANK_MAX-1){
          peer=srvs_.add_node(u,abank,auser); //deposits BANK_MIN_TMASS
          bank_fee.resize(srvs_.nodes.size());
          ELOG("BANK, add new bank %04X\n",peer);}
        else{
          //close(fd);
          ELOG("BANK, can not create more banks\n");
          //goto BLK_END;
          union {uint64_t big;uint32_t small[2];} to;
          to.small[0]=auser;
          to.small[1]=abank;
          deposit[to.big]+=BANK_MIN_TMASS;
          if(auser){
            bank_fee[to.small[1]]-=BANK_PROFIT(TXS_LNG_FEE(BANK_MIN_TMASS));} //else would generate extra fee
          if(abank==opts_.svid){
            uint64_t key=(uint64_t)auser<<32;
            key|=lpos++;
            log_t alog;
            alog.time=time(NULL);
            alog.type=TXSTYPE_BNK|0x8000; //incoming
            alog.node=0;
            alog.user=0;
            alog.umid=0;
            alog.nmid=0;
            alog.mpos=srvs_.now;
            alog.weight=BANK_MIN_TMASS;
            memcpy(alog.info,u.pkey,32);
            log[key]=alog;}
            //put_log(abank,auser,alog); //put_blklog
          hlg.save_bnk(auser,0,ppi_txid(it->first));
          continue;}
        update.insert(peer);
        if(abank==opts_.svid){
          uint64_t key=(uint64_t)auser<<32;
          key|=lpos++;
          log_t alog;
          alog.time=time(NULL);
          alog.type=TXSTYPE_BNK|0x8000; //incoming
          alog.node=peer;
          alog.user=0;
          alog.umid=0;
          alog.nmid=0;
          alog.mpos=srvs_.now;
          alog.weight=0;
          memcpy(alog.info,u.pkey,32);
          log[key]=alog;}
          //put_log(abank,auser,alog); //put_blklog
        hlg.save_bnk(auser,peer,ppi_txid(it->first));
        close(fd);}
      //BLK_END:
      blk_bnk.clear();}

    //withdraw funds
    uint16_t svid=0;
    int fd=-1;
    std::map<uint32_t,user_t> undo;
    for(auto it=blk_get.begin();it!=blk_get.end();it++){
      uint16_t abank=ppi_abank(it->first);
      uint16_t bbank=ppi_bbank(it->first);
      update.insert(abank);
      update.insert(bbank);
      if(bbank!=svid){
        srvs_.save_undo(svid,undo,0);
        undo.clear();
        svid=bbank;
        if(fd>=0){
          close(fd);}
        fd=open_bank(svid);}
      union {uint64_t big;uint32_t small[2];} to;
      to.small[1]=abank; //assume big endian
      auto tx=&it->second;
      user_t u;
      lseek(fd,tx->buser*sizeof(user_t),SEEK_SET);
      read(fd,&u,sizeof(user_t));
      if(!memcmp(u.pkey,tx->pkey,32)){ //FIXME, add transaction fees processing
        if(u.time+LOCK_TIME>srvs_.now){
          continue;}
        undo.emplace(tx->buser,u);
        int64_t fee=0;
        int64_t div=dividend(u,fee);
        if(u.weight-TXS_GOK_FEE(u.weight)<=0){
          continue;}
        bank_fee[svid]+=BANK_PROFIT(fee); // only from dividend
        if(svid==opts_.svid){
          mydiv_fee+=BANK_PROFIT(fee);}
        if(div!=(int64_t)0x8FFFFFFFFFFFFFFF){
          //DLOG("DIV: pay to %04X:%08X (%016lX)\n",bbank,tx->buser,div);
          srvs_.nodes[bbank].weight+=div;}
        int64_t delta=0;
        int64_t delta_gok=0;
        fee=0;
        log_t tlog;
        memcpy(tlog.info+ 0,&u.weight,8);
        if(u.node!=abank||u.user!=tx->auser){ //initiate withdraw
          u.node=abank;
          u.user=tx->auser;}
        else{ //commit withdraw after lockup (all funds)
          delta=u.weight;
          delta_gok=u.weight-TXS_GOK_FEE(delta);
          fee=TXS_GOK_FEE(delta);
          u.weight=0;
          srvs_.nodes[bbank].weight-=delta;
          to.small[0]=tx->auser; //assume big endian
          if(abank==opts_.svid){
            myget_fee+=BANK_PROFIT(TXS_LNG_FEE(delta_gok));}
          //bank_fee[to.small[1]]-=BANK_PROFIT(TXS_LNG_FEE(delta_gok))>>2; //reduce bank fee
          deposit[to.big]+=delta_gok;}
        u.time=srvs_.now;
        srvs_.xor4(srvs_.nodes[bbank].hash,u.csum); // weights do not change
        srvs_.user_csum(u,bbank,tx->buser);
        srvs_.xor4(srvs_.nodes[bbank].hash,u.csum);
        lseek(fd,-sizeof(user_t),SEEK_CUR);
        write(fd,&u,sizeof(user_t)); // write before undo ... not good for sync
        tlog.time=time(NULL);
        tlog.umid=0;
        tlog.nmid=0;
        tlog.mpos=srvs_.now;
        memcpy(tlog.info+ 8,&delta_gok,8); // sender_cost
        memcpy(tlog.info+16,&fee,8);
        memcpy(tlog.info+24,&u.stat,2);
        memcpy(tlog.info+26,&u.pkey,6);
        if(abank==opts_.svid){
          uint64_t key=(uint64_t)tx->auser<<32;
          key|=lpos++;
          tlog.type=TXSTYPE_GET|0x8000; //incoming
          tlog.node=bbank;
          tlog.user=tx->buser;
          tlog.weight=delta_gok;
          log[key]=tlog;}
          //put_log(abank,tx->auser,tlog); //put_blklog
        if(bbank==opts_.svid){
          uint64_t key=(uint64_t)tx->buser<<32;
          key|=lpos++;
          tlog.type=TXSTYPE_GET; //outgoing
          tlog.node=abank;
          tlog.user=tx->auser;
          tlog.weight=-delta_gok;
          log[key]=tlog;}
          //put_log(bbank,tx->buser,tlog); //put_blklog
        if(bbank==opts_.svid && !do_sync && ofip!=NULL){
          gup_t g;
          g.auser=tx->buser;
          g.node=u.node;
          g.user=u.user;
          g.time=u.time;
          g.delta=delta;
          ofip_gup_push(g);}
        hlg.save_get(tx->auser,bbank,tx->buser,ppi_txid(it->first));}}
    blk_get.clear();
    if(svid){
      if(fd>=0){ // always true
        close(fd);}
      srvs_.save_undo(svid,undo,0);
      undo.clear();}

    hash_t hash;
    int total;
    hlg.finish(hash,total);
    if(total){
      DLOG("DEBUG, written %d bytes to hlog\n",total);
      memcpy(srvs_.nodes[0].hash,hash,SHA256_DIGEST_LENGTH);}
    else{
      bzero(srvs_.nodes[0].hash,SHA256_DIGEST_LENGTH);}

    del_msglog(srvs_.now,0,0);
    put_msglog(srvs_.now,0,0,log);
  }

  int64_t dividend(user_t& u)
  { if(u.rpath<period_start && u.lpath<period_start){
      u.rpath=srvs_.now;
      int64_t div=(u.weight>>16)*srvs_.div-TXS_DIV_FEE;
      if(div<-u.weight){
        div=-u.weight;}
      u.weight+=div;
      return(div);}
    return(0x8FFFFFFFFFFFFFFF);
  }

  int64_t dividend(user_t& u,int64_t& fee)
  { if(u.rpath<period_start && u.lpath<period_start){
      u.rpath=srvs_.now;
      int64_t div=(u.weight>>16)*srvs_.div-TXS_DIV_FEE;
      if(div<-u.weight){
        div=-u.weight;}
      else{
        fee+=TXS_DIV_FEE;}
      u.weight+=div;
      return(div);}
    return(0x8FFFFFFFFFFFFFFF);
  }

  void commit_dividends(std::set<uint16_t>& update) //assume single thread, TODO change later
  { if((srvs_.now/BLOCKSEC)%BLOCKDIV<BLOCKDIV/2){
      return;}
    //uint32_t now=time(NULL); //for the log
    //std::map<uint64_t,log_t> log;
    //char filename[64];
    user_t u,ou;
    const int offset=(char*)&u+sizeof(user_t)-(char*)&u.rpath;
    assert((char*)&u.rpath<(char*)&u.weight);
    assert((char*)&u.rpath<(char*)&u.csum);
    const int shift=srvs_.now/BLOCKSEC;
    const int segment=32;
    const int period=BLOCKDIV/2;
    int bend=srvs_.nodes.size();
    std::map<uint32_t,user_t> undo;
    for(int svid=1;svid<bend;svid++){
      uint32_t user=segment*((shift+svid)%period);
      uint32_t uend=srvs_.nodes[svid].users;
      if(user>=uend){
        continue;}
      DLOG("DIVIDEND to usr/%04X.dat\n",svid);
      int fd=open_bank(svid);
      //sprintf(filename,"usr/%04X.dat",svid);
      //int fd=open(filename,O_RDWR,0644);
      //if(fd<0){
      //  ELOG("ERROR, failed to open bank register %04X, fatal\n",svid);
      //  exit(-1);}
      for(;;user+=segment*(period-1)){
        lseek(fd,user*sizeof(user_t),SEEK_SET);
        for(int i=0;i<segment;i++,user++){
          if(user>=uend){
            goto NEXTBANK;}
          read(fd,&u,sizeof(user_t));
          if(!u.msid){
            ELOG("ERROR, failed to read user %04X:%08X, fatal\n",svid,user);
            close(fd);
            exit(-1);}
          memcpy(&ou,&u,sizeof(user_t)); //to keep data for undo
          int64_t fee=0;
          int64_t div=dividend(u,fee);
          bank_fee[svid]+=BANK_PROFIT(fee);
          if(svid==opts_.svid){
            mydiv_fee+=BANK_PROFIT(fee);}
//check if account should be closed
          if(div!=(int64_t)0x8FFFFFFFFFFFFFFF || !u.weight){
            if(div==(int64_t)0x8FFFFFFFFFFFFFFF){
              div=0;}
            //DLOG("DIV: to %04X:%08X (%016lX)\n",svid,user,div);
            undo.emplace(user,ou); // no emplace needed, insert is ok
            srvs_.xor4(srvs_.nodes[svid].hash,u.csum);
            union {uint64_t big;uint32_t small[2];} to;
            to.small[0]=user;
            to.small[1]=svid;
            auto it=deposit.find(to.big);
            if(it!=deposit.end()){
              if(svid==opts_.svid && !do_sync && ofip!=NULL){
                ofip_add_remote_deposit(user,it->second);} //DEPOSIT
              u.weight+=it->second;
              div+=it->second;
              it->second=0;}
            if(u.weight<=TXS_DIV_FEE && (srvs_.now-USER_MIN_AGE>u.lpath)){ //alow deletion of account
              u.stat|=USER_STAT_DELETED;
              if(svid==opts_.svid && !do_sync && ofip!=NULL){
                ofip_delete_user(user);}}
            srvs_.user_csum(u,svid,user);
            srvs_.xor4(srvs_.nodes[svid].hash,u.csum);
            srvs_.nodes[svid].weight+=div;
            lseek(fd,-offset,SEEK_CUR);
            write(fd,&u.rpath,offset);}}} // write before undo ... not good for sync
      NEXTBANK:
      update.insert(svid);
      close(fd);
      srvs_.save_undo(svid,undo,0);
      undo.clear();}
  }

  void commit_deposit(std::set<uint16_t>& update) //assume single thread, TODO change later !!!
  { //uint32_t now=time(NULL); //for the log
    //std::map<uint64_t,log_t> log;
    //char filename[64];
    uint16_t lastsvid=0;
    int /*ud=0,*/fd=-1;
    user_t u;
    const int offset=(char*)&u+sizeof(user_t)-(char*)&u.rpath;
    assert((char*)&u.rpath<(char*)&u.weight);
    assert((char*)&u.rpath<(char*)&u.csum);
    std::map<uint32_t,user_t> undo;
    for(auto it=deposit.begin();it!=deposit.end();it++){
      if(it->second==0){ //MUST keep this to prevent rpath change, it may indicate undone transaction !
        continue;}
      union {uint64_t big;uint32_t small[2];} to;
      to.big=it->first;
      uint32_t user=to.small[0];
      uint16_t svid=to.small[1];
      assert(svid);
      if(svid!=lastsvid){
        if(fd>=0){
          close(fd);}
        srvs_.save_undo(lastsvid,undo,0);
        undo.clear();
        //FIXME, should stop sync attempts on bank file, lock bank or accept sync errors
        lastsvid=svid;
	update.insert(svid);
        DLOG("DEPOSIT to usr/%04X.dat\n",svid);
        fd=open_bank(svid);}
        //sprintf(filename,"usr/%04X.dat",svid);
        //fd=open(filename,O_RDWR,0644);
        //if(fd<0){
        //  ELOG("ERROR, failed to open bank register %04X, fatal\n",svid);
        //  exit(-1);}
      lseek(fd,user*sizeof(user_t),SEEK_SET);
      read(fd,&u,sizeof(user_t));
      undo.emplace(user,u);
      int64_t fee=0;
      int64_t div=dividend(u,fee);
      bank_fee[svid]+=BANK_PROFIT(fee);
      if(svid==opts_.svid){
        mydiv_fee+=BANK_PROFIT(fee);}
      if(div==(int64_t)0x8FFFFFFFFFFFFFFF){
        div=0;}
      else{
        //DLOG("DIV: during deposit to %04X:%08X (%016lX) (%016lX)\n",svid,user,div,it->second);
        }
      if(user){ // no fees from remote deposits to bank
        bank_fee[svid]+=BANK_PROFIT(TXS_LNG_FEE(it->second));}
      u.weight+=it->second;
      u.rpath=srvs_.now;
      if(svid==opts_.svid && !do_sync && ofip!=NULL){
        ofip_add_remote_deposit(user,it->second);} //DEPOSIT
      srvs_.xor4(srvs_.nodes[svid].hash,u.csum);
      srvs_.user_csum(u,svid,user);
      srvs_.xor4(srvs_.nodes[svid].hash,u.csum);
      srvs_.nodes[svid].weight+=it->second+div;
      lseek(fd,-offset,SEEK_CUR);
      write(fd,&u.rpath,offset);} // write before undo ... not good for sync
    if(lastsvid){
      if(fd>=0){ // always true
        close(fd);}
      srvs_.save_undo(lastsvid,undo,0);
      undo.clear();}
    deposit.clear(); //remove deposits after commiting
  }

  void commit_bankfee()
  { uint16_t max_svid=srvs_.nodes.size();
    user_t u;
    const int offset=(char*)&u+sizeof(user_t)-(char*)&u.rpath;
    assert((char*)&u.rpath<(char*)&u.weight);
    assert((char*)&u.rpath<(char*)&u.csum);
    std::map<uint64_t,log_t> log;

    for(uint16_t svid=1;svid<max_svid;svid++){
      int fd=open_bank(svid);
      //char filename[64];
      //sprintf(filename,"usr/%04X.dat",svid);
      //int fd=open(filename,O_RDWR,0644);
      //if(fd<0){
      //  ELOG("ERROR, failed to open bank register %04X, fatal\n",svid);
      //  exit(-1);}
      read(fd,&u,sizeof(user_t));
      std::map<uint32_t,user_t> undo;
      undo.emplace(0,u);
      int64_t fee=0;
      int64_t div=dividend(u,fee);
      //int64_t bank_fee_start=bank_fee[svid];
      bank_fee[svid]+=BANK_PROFIT(fee);
      if(div==(int64_t)0x8FFFFFFFFFFFFFFF){
        div=0;}
      else{
        ELOG("DIV: during bank_fee to %04X (%016lX)\n",svid,div);
        if(svid==opts_.svid){
          mydiv_fee+=BANK_PROFIT(fee);}}
      int64_t buser_fee=BANK_USER_FEE(srvs_.nodes[svid].users);
      int64_t profit=bank_fee[svid]-buser_fee;
      DLOG("PROFIT %04X %016lX (%lX+%lX-%lX)\n",svid,profit,profit-BANK_PROFIT(fee)-buser_fee,BANK_PROFIT(fee),buser_fee);
      //int64_t before=u.weight; 
      if(profit<-u.weight){
       profit=-u.weight;}
      u.weight+=profit;
      //int64_t after=u.weight; 
      assert(svid<srvs_.nodes.size());
      srvs_.nodes[svid].weight+=profit+div;
      u.rpath=srvs_.now;
      if(svid==opts_.svid && !do_sync && ofip!=NULL){
        ofip_add_remote_deposit(0,profit);} //DEPOSIT
      srvs_.xor4(srvs_.nodes[svid].hash,u.csum);
      srvs_.user_csum(u,svid,0);
      srvs_.xor4(srvs_.nodes[svid].hash,u.csum);
      lseek(fd,-offset,SEEK_CUR);
      write(fd,&u.rpath,offset); // write before undo ... not good for sync
      close(fd);
      srvs_.save_undo(svid,undo,0);
      if(svid==opts_.svid){
        log_t alog;
        alog.time=time(NULL);
        alog.type=TXSTYPE_FEE|0x8000; //incoming ... bank_fee
        alog.node=svid;
        alog.user=0;
        alog.umid=0;
        alog.nmid=0;
        alog.mpos=srvs_.now;
        alog.weight=profit;
        memcpy(alog.info,&mydiv_fee,sizeof(int64_t));
        memcpy(alog.info+sizeof(int64_t),&myusr_fee,sizeof(int64_t));
        memcpy(alog.info+2*sizeof(int64_t),&myget_fee,sizeof(int64_t));
        memcpy(alog.info+3*sizeof(int64_t),&buser_fee,sizeof(int64_t));
        log[0]=alog;}}
        //put_log(svid,0,alog);  //put_blklog
    //bank_fee.clear();
    bzero(bank_fee.data(),bank_fee.size()*sizeof(uint64_t));
    DLOG("RESET bank_fee[%ld] (bank_fee[1]=%ld)\n",bank_fee.size(),bank_fee[1]);
    put_msglog(srvs_.now,0,0,log);
  }

  //bool accept_message(uint32_t lastmsid)
  bool accept_message()
  { //FIXME, add check for vulnerable time
    dbls_.lock();
    if(dbl_srvs_.find(opts_.svid)!=dbl_srvs_.end()){
      dbls_.unlock();
      return(false);}
    dbls_.unlock();
    return(msid_==srvs_.nodes[opts_.svid].msid && !(srvs_.nodes[opts_.svid].status & SERVER_DBL));
  }

  void update_list(std::vector<uint64_t>& txs,std::vector<uint64_t>& dbl,std::vector<uint64_t>& blk,uint16_t peer_svid)
  { txs_.lock();
    for(auto me=txs_msgs_.begin();me!=txs_msgs_.end();me++){
      if((me->second->status & MSGSTAT_COM) && me->second->path==srvs_.now){
        union {uint64_t num; uint8_t dat[8];} h;
        h.num=me->first;
        h.dat[0]=MSGTYPE_PUT;
        h.dat[1]=me->second->hashval(peer_svid); //data[4+(peer_svid%64)]
        me->second->sent_erase(peer_svid);
        txs.push_back(h.num);}}
    txs_.unlock();
    dbl_.lock();
    for(auto me=dbl_msgs_.begin();me!=dbl_msgs_.end();me++){
      if((me->second->status & MSGSTAT_COM) && me->second->path==srvs_.now){
        union {uint64_t num; uint8_t dat[8];} h;
        h.num=me->first;
        h.dat[0]=MSGTYPE_DBP;
        h.dat[1]=0;//me->second->data[4+(peer_svid%64)];
        me->second->sent_erase(peer_svid);
        dbl.push_back(h.num);}}
    dbl_.unlock();
    blk_.lock();
    for(auto me=blk_msgs_.begin();me!=blk_msgs_.end();me++){
      if((me->second->status & MSGSTAT_VAL) && me->second->path==last_srvs_.now){
        union {uint64_t num; uint8_t dat[8];} h;
        h.num=me->first;
        h.dat[0]=MSGTYPE_BLP;
        h.dat[1]=me->second->hashval(peer_svid);
        me->second->sent_erase(peer_svid);
        blk.push_back(h.num);}}
    blk_.unlock();
  }

  void finish_block()
  { std::set<uint16_t> update; //useless because now all nodes are updated because of maintenance fee for admin
    commit_block(update); // process bkn and get transactions
    commit_dividends(update);
    commit_deposit(update);
    commit_bankfee();
//#ifdef DEBUG
    DLOG("CHECK accounts\n");
    for(auto it=update.begin();it!=update.end();it++){
      assert(*it<srvs_.nodes.size());
      if(!srvs_.check_nodehash(*it)){
        ELOG("FATAL ERROR, failed to check bank %04X\n",*it);
        exit(-1);}}
//#endif
    srvs_.finish(); //FIXME, add locking
    ELOG("SPEED: %.1f  [txs:%lu]\n",(float)srvs_.txs/(float)BLOCKSEC,srvs_.txs);
    last_srvs_=srvs_; // consider not making copies of nodes
    memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
    period_start=srvs_.nextblock();
    iamvip=(bool)(srvs_.nodes[opts_.svid].status & SERVER_VIP);
    //vip_max=srvs_.update_vip(); // move to nextblock()
    if(!do_sync){
      ofip_update_block(period_start,srvs_.now,LAST_block_final_msgs,srvs_.div);
      //free(hash);
      cnd_.lock();
      for(auto mj=cnd_msgs_.begin();mj!=cnd_msgs_.end();){
        auto mi=mj++;
        if(mi->second->msid<last_srvs_.now){
          cnd_msgs_.erase(mi);
          continue;}}
      cnd_.unlock();
      blk_.lock();
      for(auto mj=blk_msgs_.begin();mj!=blk_msgs_.end();){
        auto mi=mj++;
        if(mi->second->msid<last_srvs_.now){
          blk_msgs_.erase(mi);
          continue;}}
      blk_.unlock();
      dbls_.lock();
      dbl_srvs_.clear();
      dbls_.unlock();}
    DLOG("NEW BLOCK created\n");
    srvs_.clean_old(opts_.svid);
  }

  //message_ptr write_handshake(uint32_t ipv4,uint32_t port,uint16_t peer)
  message_ptr write_handshake(uint16_t peer,handshake_t& hs)
  { last_srvs_.header(hs.head); //
    last_srvs_.header_print(hs.head);
    if(peer){
      if(!do_sync){
        memcpy(hs.msha,srvs_.nodes[peer].msha,SHA256_DIGEST_LENGTH);
        hs.msid=srvs_.nodes[peer].msid;}
      else{
        memcpy(hs.msha,last_srvs_.nodes[peer].msha,SHA256_DIGEST_LENGTH);
        hs.msid=last_srvs_.nodes[peer].msid;}}
    else{
      //if(do_fast){ // force sync if '-f 1'
      //  hs.vno=0xFFFF;}
      bzero(hs.msha,SHA256_DIGEST_LENGTH);
      if(do_fast){
        hs.msid=0xFFFFFFFF;} // not needed
      else{
        hs.msid=0;}}
    hs.do_sync=do_sync;
    //ed25519_printkey(skey,32);
    //ed25519_printkey(pkey,32);
    hash_t empty;
    message_ptr msg(new message(MSGTYPE_INI,(uint8_t*)&hs,(int)sizeof(handshake_t),opts_.svid,msid_,skey,pkey,empty));
    return(msg);
  }

#ifdef DOUBLE_SPEND
  uint32_t write_dblspend(std::string line)
  { if(last_srvs_.nodes[opts_.svid].status & SERVER_DBL){
      DLOG("DEBUG, server marked as double spender\n");
      return(0);}
    if(dbl_srvs_.find(opts_.svid)!=dbl_srvs_.end()){
      DLOG("DEBUG, server already double spent\n");
      return(0);}
    std::vector<uint16_t> peers;
    std::set<uint64_t> nums;
    connected(peers);
    if(peers.empty()){
      return(0);}
    if(srvs_.nodes[opts_.svid].msid!=msid_){
      DLOG("ERROR, wrong network msid, postponing message write\n");
      return(0);}
    memcpy(msha_,srvs_.nodes[opts_.svid].msha,sizeof(hash_t));
    int msid=++msid_; // can be atomic
    for(auto pi : peers){
      usertxs txs(TXSTYPE_CON,opts_.port&0xFFFF,opts_.ipv4,0);
      line.append((char*)txs.data,txs.size);
      message_ptr msg(new message(MSGTYPE_MSG,(uint8_t*)line.c_str(),(int)line.length(),opts_.svid,msid,skey,pkey,msha_));
      if(nums.find(msg->hash.num)!=nums.end()){
        DLOG("OOPS duplicated double spend %04X:%08X to %04X [%016lX]\n",opts_.svid,msid,pi,msg->hash.num);
        continue;}
      nums.insert(msg->hash.num);
      DLOG("NICE sending double spend %04X:%08X to %04X [%016lX]\n",opts_.svid,msid,pi,msg->hash.num);
      msg->status|=MSGSTAT_BAD;
      txs_insert(msg);
      update(msg,pi);}
    writemsid();
    return(msid_);
  }
#endif

  uint32_t write_message(std::string line) // assume single threaded
  { if(srvs_.nodes[opts_.svid].msid!=msid_){
      DLOG("ERROR, wrong network msid, postponing message write\n");
      return(0);}
    memcpy(msha_,srvs_.nodes[opts_.svid].msha,sizeof(hash_t));
    // add location info. FIXME, set location to 0 before exit
    usertxs txs(TXSTYPE_CON,opts_.port&0xFFFF,opts_.ipv4,0);
    line.append((char*)txs.data,txs.size);
    int msid=++msid_; // can be atomic
    message_ptr msg(new message(MSGTYPE_MSG,(uint8_t*)line.c_str(),(int)line.length(),opts_.svid,msid,skey,pkey,msha_));
    memcpy(msha_,msg->sigh,sizeof(hash_t));
    if(!txs_insert(msg)){
      ELOG("FATAL message insert error for own message, dying !!!\n");
      exit(-1);}
    writemsid();
    return(msid_);
    //update(msg);
  }

  void write_candidate(const hash_s& cand_hash)
  { do_vote=0;
    hash_t empty;
    cand_.lock(); //lock only candidates
    auto ca=candidates_.find(cand_hash);
    assert(ca!=candidates_.end());
    cand_.unlock();
    message_ptr msg(new message(MSGTYPE_CND,cand_hash.hash,sizeof(hash_t),opts_.svid,srvs_.now,skey,pkey,empty)); //FIXME, consider msid=0 ???
//FIXME, is hash ok ?
    if(!ca->second->msg_add.empty() || !ca->second->msg_del.empty()){ // same function as in peer.hpp
      uint32_t num_add=ca->second->msg_add.size();
      uint32_t num_del=ca->second->msg_del.size();
      msg->len=message::data_offset+sizeof(hash_t)+4+4+num_add*sizeof(msidsvidhash_t)+num_del*6;
      msg->data=(uint8_t*)realloc(msg->data,msg->len); // throw if no RAM ???
      memcpy(msg->data+1,&msg->len,3); // remember lentgh
      memcpy(msg->data+message::data_offset+sizeof(hash_t),&num_del,4);
      memcpy(msg->data+message::data_offset+sizeof(hash_t)+4,&num_add,4);
      uint8_t* d=msg->data+message::data_offset+sizeof(hash_t)+4+4;
      for(auto it=ca->second->msg_del.begin();it!=ca->second->msg_del.end();it++,d+=6){
        memcpy(d,((char*)&(*it))+2,6);}
      for(auto it=ca->second->msg_add.begin();it!=ca->second->msg_add.end();it++,d+=sizeof(msidsvidhash_t)){
        memcpy(d,((char*)&it->first)+2,6);
        memcpy(d+6,&it->second.hash,sizeof(hash_t));}
      DLOG("0000 CHANGE CAND LENGTH! [len:%d->%d]\n",(int)(message::data_offset+sizeof(hash_t)),msg->len);
      assert((uint32_t)(d-msg->data)==msg->len);}
    if(!cnd_insert(msg)){
      ELOG("FATAL message insert error for own message, dying !!!\n");
      exit(-1);}
    ELOG("SENDING candidate\n");
    update(msg); // update peers even if we are not an elector
  }

  candidate_ptr save_candidate(uint32_t blk,const hash_s& h,std::map<uint64_t,hash_s>& add,std::set<uint64_t>& del,uint16_t peer)
  { extern message_ptr nullmsg;
    cand_.lock(); //lock only candidates
    auto ca=candidates_.find(h);
    if(ca==candidates_.end()){
      bool failed=false;
      for(auto key : del){
        uint32_t msid=(key>>16) & 0xFFFFFFFFL;
        uint16_t svid=(key>>48);
        if(msid==0xFFFFFFFF){
          ELOG("%04X WARNING peer removed DBL spend from%04X\n",peer,svid);
          failed=true;
          break;}
        message_ptr pm=message_svidmsid(svid,msid); //LOCK: txs_
        if(pm!=nullmsg && (pm->status & (MSGSTAT_DAT|MSGSTAT_VAL)) && pm->got<srvs_.now+BLOCKSEC-MESSAGE_MAXAGE){
          ELOG("%04X WARNING peer removed old message %04X:%08X\n",peer,svid,msid);
          failed=true;
          break;}}
      std::set<uint64_t> mis;
      for(auto jt=add.begin();jt!=add.end();){
        auto it=jt++;
        assert(del.find(it->first)==del.end()); //TODO, remove later
        auto me=LAST_block_all_msgs.find(it->first); //is this ready ???
        if(me!=LAST_block_all_msgs.end()){
          if(!memcmp(me->second->sigh,it->second.hash,sizeof(hash_t))){
            add.erase(it);} // no need to add this message we have it
          else{
            mis.insert(it->first);
            del.insert(me->first);}
          continue;}
        uint32_t msid=(it->first>>16) & 0xFFFFFFFFL;
        if(msid==0xFFFFFFFF){ //dbl spend marker
          continue;}
        uint16_t svid=(it->first>>48);
        if(svid>=last_srvs_.nodes.size()){
          DLOG("%04X WARNING illegal candidate (svid:%04X)\n",peer,svid);
          failed=true;
          mis.insert(it->first);
          continue;}
        if(msid<=last_srvs_.nodes[svid].msid){
          DLOG("%04X WARNING illegal candidate (%04X:%08X too old)\n",peer,svid,msid);
          failed=true;
          mis.insert(it->first);
          continue;}
        message_ptr pm=message_svidmsid(svid,msid); //LOCK: txs_
        if(pm==nullmsg || !(pm->status & (MSGSTAT_DAT)) || memcmp(pm->sigh,it->second.hash,sizeof(hash_t)) || !(pm->status & MSGSTAT_DAT)){
          mis.insert(it->first);
          continue;}}
      DLOG("%04X SAVE CANDIDATE add:%d del:%d mis:%d failed:%d\n",
        peer,(int)add.size(),(int)del.size(),(int)mis.size(),failed);
      candidate_ptr c_ptr(new candidate(blk,add,del,mis,peer,failed));
      candidates_[h]=c_ptr;
      cand_.unlock();
      return(c_ptr);}
    else if(peer){
      ca->second->peers.insert(peer);}
    cand_.unlock();
    return(ca->second);
  }

  candidate_ptr known_candidate(const hash_s& h,uint16_t peer)
  { extern candidate_ptr nullcnd;
    cand_.lock(); // lock only candidates
    auto it=candidates_.find(h);
    if(it==candidates_.end()){
      cand_.unlock();
      return(nullcnd);}
    if(peer){
      it->second->peers.insert(peer);}
    cand_.unlock();
    return(it->second);
  }

  void update_candidates(message_ptr msg)
  { cand_.lock();
    if(do_block>0){ // update candidates, check if this message was not missing
      for(auto it=candidates_.begin();it!=candidates_.end();it++){
        it->second->update(msg);}}
    cand_.unlock();
  }

  bool known_elector(uint16_t svid)
  { return(electors.find(svid)!=electors.end());
  }

  void write_header()
  { header_t head;
    last_srvs_.header(head);
    hash_t empty;
    message_ptr msg(new message(MSGTYPE_BLK,(uint8_t*)&head,sizeof(header_t),opts_.svid,head.now,skey,pkey,empty));
    if(!blk_insert(msg)){
      ELOG("FATAL message insert error for own message, dying !!!\n");
      exit(-1);}
    DLOG("SENDING block (update)\n");
    //deliver(msg);
    update(msg); //send, even if I am not VIP
// save signature in signature lists
//FIXME, save only if I am important
  }

  void peers() // connect new peers
  { 
    std::set<uint16_t> list;
    peers_known(list);
    for(std::string addr : opts_.peer){
      uint16_t peer=opts_.get_svid(addr);
      if(peer && list.find(peer)==list.end()){
        connect(addr);
        boost::this_thread::sleep(boost::posix_time::seconds(1)); //wait some time before connecting to more peers
        RETURN_ON_SHUTDOWN();}}
    while(1){
      boost::this_thread::sleep(boost::posix_time::seconds(5)); //will be interrupted to return
      RETURN_ON_SHUTDOWN();
      peer_clean(); //cleans all peers with killme==true
      //uint32_t now=time(NULL)+5; // do not connect if close to block creation time
      //now-=now%BLOCKSEC;
#ifdef DEBUG
      if(peers_.size()>=2 || peers_.size()>(srvs_.nodes.size()-2)/2 /*|| srvs_.now<now*/){
        continue;}
#else
      if(peers_.size()>=MIN_PEERS || peers_.size()>(srvs_.nodes.size()-2)/2 /*|| srvs_.now<now*/){
        continue;}
#endif
      list.clear();
      peers_known(list);
      for(std::string addr : opts_.peer){
        uint16_t peer=opts_.get_svid(addr);
        if(peer && list.find(peer)==list.end()){
          list.insert(peer);
          DLOG("TRY CONNECT to %04X (%s)\n",peer,addr.c_str());
          connect(addr);
          boost::this_thread::sleep(boost::posix_time::seconds(1)); //wait some time before connecting to more peers
          RETURN_ON_SHUTDOWN();}}
      int16_t peer=(((uint64_t)random())%srvs_.nodes.size())&0xFFFF;
      if(!peer || peer==opts_.svid || !srvs_.nodes[peer].ipv4 || !srvs_.nodes[peer].port){
        //DLOG("IGNORE CONNECT to %04X (%08X:%08X)\n",peer,srvs_.nodes[peer].ipv4,srvs_.nodes[peer].port);
        continue;}
      if(list.find(peer)!=list.end()){
        //DLOG("ALREADY CONNECT to %04X (%08X:%08X)\n",peer,srvs_.nodes[peer].ipv4,srvs_.nodes[peer].port);
        continue;}
      DLOG("TRY CONNECT to %04X (%08X:%08X)\n",peer,srvs_.nodes[peer].ipv4,srvs_.nodes[peer].port);
      connect(peer);}
  }

  void clock()
  { 
    //while(ofip==NULL){
    //  boost::this_thread::sleep(boost::posix_time::seconds(1));}
    //start office

    //TODO, number of validators should depend on opts_.
    block_only=false;
    if(!do_validate){
      do_validate=1;
      for(int v=0;v<VALIDATORS;v++){
        threadpool.create_thread(boost::bind(&server::validator, this));}}

    //TODO, consider validating local messages faster to limit delay in this region
    //finish recycle submitted office messages
    while(msid_>srvs_.nodes[opts_.svid].msid){
//FIXME, hangs sometimes !!!
      ELOG("DEBUG, waiting to process local messages (%08X>%08X)\n",msid_,srvs_.nodes[opts_.svid].msid);
      boost::this_thread::sleep(boost::posix_time::seconds(1));
      RETURN_ON_SHUTDOWN();}
    //recycle office message queue
    std::string line;
    if(ofip_get_msg(msid_+1,line)){
      while(msid_>srvs_.nodes[opts_.svid].msid){
        ELOG("DEBUG, waiting to process local messages (%08X>%08X)\n",msid_,srvs_.nodes[opts_.svid].msid);
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        RETURN_ON_SHUTDOWN();}
      ELOG("DEBUG, adding office message queue (%08X)\n",msid_+1);
      if(!write_message(line)){
        ELOG("DEBUG, failed to add office message (%08X), fatal\n",msid_+1);
        exit(-1);}
      while(msid_>srvs_.nodes[opts_.svid].msid){
        ELOG("DEBUG, waiting to process office message (%08X>%08X)\n",msid_,srvs_.nodes[opts_.svid].msid);
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        RETURN_ON_SHUTDOWN();}
      ofip_del_msg(msid_);}
    //must do this after recycling messages !!!
    ofip_init(srvs_.nodes[opts_.svid].users); // this can be slow :-(
    ofip_start();

    //TODO, if we are out of sync we could consider starting again now possibly with no local messages

    // block creation cycle
    hash_s cand;
    while(1){
      uint32_t now=time(NULL);
      const char* plist=peers_list();
      if(missing_msgs_.size()){
        ELOG("CLOCK: %02lX (check:%d wait:%d peers:%d hash:%8X now:%8X msg:%u txs:%lu) [%s] (miss:%d:%016lX)\n",
          ((long)(srvs_.now+BLOCKSEC)-(long)now),(int)check_msgs_.size(),
          //(int)wait_msgs_.size(),(int)peers_.size(),(uint32_t)*((uint32_t*)srvs_.nowhash),srvs_.now,plist,
          (int)wait_msgs_.size(),(int)peers_.size(),srvs_.nowh32(),srvs_.now,srvs_.msg,srvs_.txs,plist,
          (int)missing_msgs_.size(),missing_msgs_.begin()->first);}
      else{
        ELOG("CLOCK: %02lX (check:%d wait:%d peers:%d hash:%8X now:%8X msg:%u txs:%lu) [%s]\n",
          ((long)(srvs_.now+BLOCKSEC)-(long)now),(int)check_msgs_.size(),
          (int)wait_msgs_.size(),(int)peers_.size(),srvs_.nowh32(),srvs_.now,srvs_.msg,srvs_.txs,plist);}
      if(now>=(srvs_.now+BLOCKSEC) && do_block==0){
        DLOG("STOPing validation to start block\n");
        do_validate=0;
        threadpool.join_all();
        busy_msgs_.clear();
        DLOG("STOPed validation to start block\n");
        //create message hash
        //last_svid_dbl_set();
        //svid_.lock();
        //last_svid_msgs.swap(svid_msgs_);
        //clean_last_svid_msgs(last_svid_msgs);
        //svid_msgs_.clear();
        //svid_.unlock();
        //TODO, maybe last_svid_msgs not needed (this can be read from txs_msgs_[_VAL] when creatin block_all_msgs)
        RETURN_ON_SHUTDOWN();
        LAST_block_msgs();
        message_shash(cand.hash,LAST_block_all_msgs);
        { message_ptr put_msg(new message(1+SHA256_DIGEST_LENGTH));
          put_msg->data[0]=MSGTYPE_STP;
          memcpy(put_msg->data+1,cand.hash,SHA256_DIGEST_LENGTH);
          char hash[2*SHA256_DIGEST_LENGTH]; hash[2*SHA256_DIGEST_LENGTH-1]='?';
          ed25519_key2text(hash,put_msg->data+1,SHA256_DIGEST_LENGTH);
          ELOG("LAST HASH put %.*s\n",(int)(2*SHA256_DIGEST_LENGTH),hash);
          deliver(put_msg); // sets BLOCK_MODE for peers
        }
        prepare_poll(); // sets do_vote, clears candidates and electors
        do_block=1; //must be before save_candidate
        std::map<uint64_t,hash_s> msg_add; // could be also svid_msha
        std::set<uint64_t> msg_del; // could be also svid_msha
        save_candidate(srvs_.now,cand,msg_add,msg_del,opts_.svid); // do after prepare_poll
        do_validate=1;
        for(int v=0;v<VALIDATORS;v++){
          threadpool.create_thread(boost::bind(&server::validator, this));}}
      if(do_block>0 && do_block<3){
        RETURN_ON_SHUTDOWN();
        count_votes(now,cand);}
      if(do_block==3){
        DLOG("STOPing validation to finish block\n");
        do_validate=0;
        threadpool.join_all();
        busy_msgs_.clear();
        DLOG("STOPed validation to finish block\n");
        RETURN_ON_SHUTDOWN();
        finish_block();
        //writelastpath();
        writemsid();
        //svid_.lock();
        //svid_msgs_.clear();
        //svid_.unlock();
        write_header(); // send new block signature
        do_block=0;
        do_validate=1;
        block_only=false;
        for(int v=0;v<VALIDATORS;v++){
          threadpool.create_thread(boost::bind(&server::validator, this));}}
      boost::this_thread::sleep(boost::posix_time::seconds(1));
      RETURN_ON_SHUTDOWN();
    }
  }

  bool break_silence(uint32_t now,std::string& message,uint32_t& tnum) // will be obsolete if we start tolerating empty blocks
  { static uint32_t do_hallo=0;
    static uint32_t del=0;
//#ifdef DEBUG
    if((!(opts_.svid%2) && !(rand()%4)) || now==srvs_.now+del) // send message every 4s
//#else
//    if(!do_block && do_hallo!=srvs_.now && now<srvs_.now+BLOCKSEC && now-srvs_.now>(uint32_t)(BLOCKSEC/4+opts_.svid*VOTE_DELAY) && svid_msgs_.size()<MIN_MSGNUM)
//#endif
    { DLOG("SILENCE, sending void message due to silence\n");
//#ifdef DEBUG
      if(rand()%2){
        usertxs txs(TXSTYPE_CON,opts_.port&0xFFFF,opts_.ipv4,0);
        message.append((char*)txs.data,txs.size);}
      else{
        usertxs txs((uint32_t)(rand()%32));
        message.append((char*)txs.data,txs.size);}
//#else
//      usertxs txs(TXSTYPE_CON,opts_.port&0xFFFF,opts_.ipv4,0);
//      message.append((char*)txs.data,txs.size);
//#endif
      tnum++;
      do_hallo=srvs_.now;
      del=rand()%(2*BLOCKSEC);
      return(true);}
    if(do_hallo<srvs_.now-BLOCKSEC){
      do_hallo=srvs_.now-BLOCKSEC;
      del=rand()%(2*BLOCKSEC);}
    return(false);
  }

//struct hash_cmp
//{ bool operator()(const hash_s& i,const hash_s& j) const {int k=memcmp(i.hash,j.hash,sizeof(hash_t)); return(k<0);}
//};

  void missing_msgs_erase(message_ptr& msg)
  { missing_.lock();
    missing_msgs_.erase(msg->hash.num);
    missing_.unlock();
  }

  void missing_msgs_insert(message_ptr& msg)
  { if(msg->msid==0xFFFFFFFF){
      return;}
    missing_.lock();
    missing_msgs_[msg->hash.num]=msg;
    missing_.unlock();
  }

  int check_msgs_size()
  { return(check_msgs_.size());
  }

  uint32_t srvs_now()
  { return(srvs_.now);
  }

  void join(peer_ptr participant);
  //void leave(peer_ptr participant);
  void peer_set(std::set<peer_ptr>& peers);
  void peer_clean();
  void disconnect(uint16_t svid);
  const char* peers_list();
  void peers_known(std::set<uint16_t>& list);
  void connected(std::vector<uint16_t>& list);
  int duplicate(peer_ptr participant);
  int deliver(message_ptr msg,uint16_t svid);
  void deliver(message_ptr msg);
  int update(message_ptr msg,uint16_t svid);
  void update(message_ptr msg);
  //void svid_msid_rollback(message_ptr msg);
  void start_accept();
  void peer_accept(peer_ptr new_peer,const boost::system::error_code& error);
  void connect(std::string peer_address);
  void connect(uint16_t svid);
  void fillknown(message_ptr msg);
  void peerready(std::set<uint16_t>& ready);
  void get_more_headers(uint32_t now);
  void ofip_gup_push(gup_t& g);
  void ofip_add_remote_deposit(uint32_t user,int64_t weight);
  void ofip_init(uint32_t myusers);
  void ofip_start();
  bool ofip_get_msg(uint32_t msid,std::string& line);
  void ofip_del_msg(uint32_t msid);
  void ofip_update_block(uint32_t period_start,uint32_t now,message_map& commit_msgs,uint32_t newdiv);
  void ofip_process_log(uint32_t now);
  void ofip_add_remote_user(uint16_t abank,uint32_t auser,uint8_t* pkey);
  void ofip_delete_user(uint32_t user);
  void ofip_change_pkey(uint8_t* user);

  //FIXME, move this to servers.hpp
  //std::set<uint16_t> last_svid_dbl; //list of double spend servers in last block
  std::map<uint16_t,message_ptr> LAST_block_svid_msgs;
  message_map LAST_block_final_msgs;
  message_map LAST_block_all_msgs;
  uint32_t LAST_block;
  //std::map<uint16_t,message_ptr> svid_msgs_; //last validated txs message or dbl message from server
  //boost::mutex svid_;

  //FIXME, use serv_.now instead
  servers last_srvs_;
  //message_ptr block; // my block message, now data in last_srvs_
  int do_sync;
  int do_fast;
  int do_check;
  //int get_headers; //TODO :-( reduce the number of flags
  uint32_t msid_; // change name to msid :-)
  //int vip_max; // user last_srvs_.vtot;
  std::list<servers> headers; //FIXME, make this private
  uint32_t get_msglist; //block id of the requested msglist of messages
  office* ofip;
  uint8_t *pkey; //used by office/client to create BKY transaction
  uint32_t start_path;
  uint32_t start_msid;
private:
  hash_t skey;
  //enum { max_connections = 4 }; //unused
  //enum { max_recent_msgs = 1024 }; // this is the block chain :->
  boost::asio::ip::tcp::endpoint endpoint_;
  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  boost::asio::ip::tcp::acceptor acceptor_;
  servers srvs_;
  options& opts_;
  boost::thread* ioth_;
  boost::thread_group threadpool;
  boost::thread* peers_thread;
  boost::thread* clock_thread;
  //uint8_t lasthash[SHA256_DIGEST_LENGTH]; // hash of last block, this should go to path/servers.txt
  //uint8_t prevhash[SHA256_DIGEST_LENGTH]; // hash of previous block, this should go to path/servers.txt
  int do_validate; // keep validation threads running
  //uint32_t maxnow; // do not process messages if time >= maxnow
  candidate_ptr winner; // elected candidate
  std::set<message_ptr> busy_msgs_; // messages in validation
  std::set<peer_ptr> peers_;
  std::map<hash_s,candidate_ptr,hash_cmp> candidates_; // list of candidates, TODO should be map of message_ptr
  message_queue wait_msgs_;
  message_queue check_msgs_;
  std::map<hash_s,message_ptr,hash_cmp> bad_msgs_;
  message_map missing_msgs_; //TODO, start using this, these are messages we still wait for
  message_map txs_msgs_; //_TXS messages (transactions)
  message_map ldc_msgs_; //_TXS messages (transactions in sync mode)
  message_map cnd_msgs_; //_CND messages (block candidates)
  message_map blk_msgs_; //_BLK messages (blocks) or messages in a block when syncing
  message_map dbl_msgs_; //_DBL messages (double spend)

  // voting
  std::map<uint16_t,uint64_t> electors;
  uint64_t votes_max;
  int do_vote;
  int do_block;
  std::map<uint64_t,int64_t> deposit;
  std::map<uint64_t,uint16_t> blk_bky; //create new bank
  std::map<uint64_t,uint32_t> blk_bnk; //create new bank
  std::map<uint64_t,get_t> blk_get; //set lock / withdraw
  std::map<uint64_t,usr_t> blk_usr; //remote account request
  std::map<uint64_t,uok_t> blk_uok; //remote account accept
  std::map<uint64_t,uint32_t> blk_sbs; //set node status bits
  std::map<uint64_t,uint32_t> blk_ubs; //clear node status bits
  std::vector<int64_t> bank_fee;
  int64_t mydiv_fee; // just to record the local TXS_DIV_FEE bank income
  int64_t myusr_fee; // just to record the local TXS_DIV_FEE bank income
  int64_t myget_fee; // just to record the local TXS_DIV_FEE bank income
  uint32_t period_start; //start time of this period
  hash_t msha_;
  std::set<uint16_t> dbl_srvs_; //list of detected double servers
  bool iamvip;
  bool block_only;

  boost::mutex cand_;
  boost::mutex wait_;
  boost::mutex check_;
  boost::mutex bad_;
  boost::mutex missing_;
  boost::mutex txs_;
  boost::mutex ldc_;
  boost::mutex cnd_;
  boost::mutex blk_;
  boost::mutex dbls_;
  boost::mutex dbl_;
  boost::mutex deposit_;
  boost::mutex headers_;
  boost::mutex peer_;
};

#endif // SERVER_HPP
