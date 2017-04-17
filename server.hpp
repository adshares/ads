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
    do_fast(1),
    ofip(NULL),
    endpoint_(boost::asio::ip::tcp::v4(),opts.port),	//TH
    io_service_(),
    work_(io_service_),
    acceptor_(io_service_,endpoint_),
    opts_(opts),
    do_validate(0),
    votes_max(0.0),
    do_vote(0),
    do_block(0)
  {
  }

  ~server()
  { //do_validate=0;
    //threadpool.join_all();
    //clock_thread->interrupt();
    //clock_thread->join();
    std::cerr<<"Server down\n";
  }

  void start()
  { mkdir("usr",0755); // create dir for bank accounts
    mkdir("blk",0755); // create dir for blocks
    uint32_t path=readmsid()-opts_.back*BLOCKSEC; // reads msid_ and path, FIXME, do not read msid, read only path
    uint32_t lastpath=path;
    //remember start status
    start_path=path;
    start_msid=msid_;
    last_srvs_.get(path);
    bank_fee.resize(last_srvs_.nodes.size());
    pkey=last_srvs_.nodes[opts_.svid].pk;

    if(opts_.init){
      struct stat sb;   
      uint32_t now=time(NULL);
      now-=now%BLOCKSEC;
      if(stat("usr/0001.dat",&sb)>=0){
        LOG("INIT from last state @ %08X with MSID: %08X (file usr/0001.dat found)\n",path,msid_);
        //LOG("INIT from last state @ %08X with MSID: %08X (file usr/0001.dat found)\n",path,msid_);
        if(!undo_bank()){
          LOG("ERROR loading initial database, fatal\n");
          exit(-1);}
        // do not rebuild blockchain
        //last_srvs_.now=now;
        //last_srvs_.blockdir();
        //last_srvs_.finish();
        // rebuild blockchain
        srvs_=last_srvs_;
        memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
        period_start=srvs_.nextblock();
        LOG("MAKE BLOCKCHAIN\n");
        for(;srvs_.now<now;){
          finish_block();}}
      else{
        path=0;
        lastpath=0;
        start_path=0;
        start_msid=0;
        msid_=0;
        LOG("START from a fresh database\n");
        last_srvs_.init(now-BLOCKSEC);
        srvs_=last_srvs_;
        memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
        period_start=srvs_.nextblock();} //changes now!
      do_sync=0;}
    else{
      srvs_=last_srvs_;
      memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
      period_start=srvs_.nextblock();} //changes now!

    LOG("START @ %08X with MSID: %08X\n",path,msid_);
    vip_max=srvs_.update_vip(); //based on initial weights at start time

    if(last_srvs_.nodes.size()<=(unsigned)opts_.svid){ 
      std::cerr << "ERROR: reading servers\n";
      exit(-1);} 
    if(!last_srvs_.find_key(pkey,skey)){
      char pktext[2*32+1]; pktext[2*32]='\0';
      ed25519_key2text(pktext,pkey,32);
      LOG("ERROR: failed to find secret key for key:\n%.16s\n",pktext);
      exit(-1);}

    if(!opts_.init){
      if(!undo_bank()){//check database consistance
        if(!opts_.fast){
          std::cerr<<"DATABASE check failed, must use fast option to load new datase from network\n";
          exit(-1);}
        do_sync=1;}
      else{
        std::cerr<<"DATABASE check passed\n";
        uint32_t now=time(NULL);
        now-=now%BLOCKSEC;
        if(last_srvs_.now==now-BLOCKSEC){
          do_sync=0;}
        else{
          if(last_srvs_.now<now-MAXLOSS && !opts_.fast){
            std::cerr<<"WARNING, possibly missing too much history for full resync\n";}
          do_sync=1;}}}
    //ofip_init(last_srvs_.nodes[opts_.svid].users); //must do this after recycling messages !!!

    //FIXME, move this to a separate thread that will keep a minimum number of connections
    ioth_ = new boost::thread(boost::bind(&server::iorun, this));
    for(std::string addr : opts_.peer){
      //std::cerr<<"CONNECT :"<<addr<<"\n";
      connect(addr);
      boost::this_thread::sleep(boost::posix_time::seconds(2));} //wait some time before connecting to more peers
    peers_thread = new boost::thread(boost::bind(&server::peers, this));

    if(do_sync){
      if(opts_.fast){ //FIXME, do slow sync after fast sync
        while(do_fast){ // fast_sync changes the status, FIXME use future/promis
          boost::this_thread::sleep(boost::posix_time::seconds(1));}
        //wait for all user files to arrive
        load_banks();}
      else{
        do_fast=0;}
      std::cerr<<"START syncing headers\n";
      load_chain(); // sets do_sync=0;
      svid_msgs_.clear();}
    //load old messages or check/modify

    recyclemsid(lastpath+BLOCKSEC);
    writemsid(); // synced to new position
    clock_thread = new boost::thread(boost::bind(&server::clock, this));
    start_accept();
  }

  void iorun()
  { while(1){
      try{
        std::cerr << "Server.Run starting\n";
        io_service_.run();
        std::cerr << "Server.Run finished\n";
        return;} //Now we know the server is down.
      catch (std::exception& e){
        std::cerr << "Server.Run error: " << e.what() << "\n";}}
  }
  void stop()
  { do_validate=0;
    io_service_.stop();
    ioth_->join();
    threadpool.join_all();
    peers_thread->interrupt();
    peers_thread->join();
    clock_thread->interrupt();
    clock_thread->join();
    std::cerr<<"Shutting down completed\n";
  }

  void recyclemsid(uint32_t lastpath)
  { uint32_t firstmsid=srvs_.nodes[opts_.svid].msid;
    hash_t msha;
    if(firstmsid>msid_){
      LOG("ERROR initial msid lower than on network, fatal (%08X<%08X)\n",msid_,firstmsid);
      if(!opts_.fast){
        exit(-1);}
      msid_=firstmsid;
      return;}
    if(firstmsid==msid_){
      std::cerr<<"NO recycle needed\n";
      return;}
    std::cerr<<"START recycle\n";
    memcpy(msha,srvs_.nodes[opts_.svid].msha,sizeof(hash_t));
    firstmsid++;
    uint32_t ntime=0;
    for(uint32_t lastmsid=firstmsid;lastmsid<=msid_;lastmsid++){
      message_ptr msg(new message(MSGTYPE_MSG,lastpath,opts_.svid,lastmsid,pkey,msha)); //load from file
      if(msg->status!=MSGSTAT_DAT){
        LOG("ERROR, failed to read message %08X/%02x_%04x_%08x.txt\n",
          lastpath,MSGTYPE_MSG,opts_.svid,lastmsid);
        msid_=lastmsid-1;
        return;}
      if(msg->now<last_srvs_.now || ntime){ //FIXME, must sign this message again if message too old !!!
        //TODO, check double spend 
        if(!ntime){
	  ntime=time(NULL);
          assert(ntime>=srvs_.now);}
        LOG("RECYCLED message %04X:%08X from %08X/ signing with new time %08X\n",opts_.svid,lastmsid,lastpath,ntime);
        msg->signnewtime(ntime,skey,pkey,msha);
        ntime++;}
      memcpy(msha,msg->sigh,sizeof(hash_t));
      if(txs_insert(msg)){
        LOG("RECYCLED message %04X:%08X from %08X/ inserted\n",opts_.svid,lastmsid,lastpath);
	if(srvs_.now!=lastpath){
          LOG("MOVE message %04X:%08X from %08X/ to %08X/\n",opts_.svid,lastmsid,lastpath,srvs_.now);
          msg->move(srvs_.now);}}
      else{
        LOG("RECYCLED message %04X:%08X from %08X/ known\n",opts_.svid,lastmsid,lastpath);}}
    if(msid_!=start_msid){
      LOG("ERROR, failed to get correct msid now:%08X<>start:%08X, check msid.txt\n",msid_,start_msid);
      exit(-1);}
    //std::cerr<<"FINISH recycle (remove old files)\n";
    //if(srvs_.now!=lastpath){
    //  message_ptr msg(new message());
    //  msg->path=lastpath;
    //  msg->hashtype(MSGTYPE_MSG);
    //  msg->svid=opts_.svid;
    //  for(;firstmsid<=msid_;firstmsid++){
    //    msg->msid=firstmsid;
    //    LOG("REMOVING message %04X:%08X from %08X/\n",msg->svid,msg->msid,lastpath);
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
    if(svid){
      sprintf(filename,"blk/%03X/%05X/log/%04X_%08X.log",now>>20,now&0xFFFFF,svid,msid);
      fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);}
    else{
      sprintf(filename,"blk/%03X/%05X/log/%04X_%08X.log",now>>20,now&0xFFFFF,0,0);
      fd=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644);}
    if(fd<0){
      std::cerr<<"ERROR, failed to open log file "<<filename<<"\n";
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
    LOG("CHECK DATA @%08X (and undo till @%08X)\n",last_srvs_.now,path+rollback*BLOCKSEC);
    bool failed=false;
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
        LOG("USING bank %04X block %08X undo %s\n",bank,npath,filename);
        ud.push_back(nd);}
      uint32_t users=last_srvs_.nodes[bank].users;
       int64_t weight=0;
      //SHA256_CTX sha256;
      //SHA256_Init(&sha256);
      uint64_t csum[4]={0,0,0,0};
      for(uint32_t user=0;user<users;user++){
        user_t u;
        //if(ud>=0){
        for(auto it=ud.begin();it!=ud.end();it++){
          u.msid=0;
          if(sizeof(user_t)==read(*it,&u,sizeof(user_t)) && u.msid){
            LOG("OVERWRITE: %04X:%08X (weight:%016lX)\n",bank,user,u.weight);
            write(fd,&u,sizeof(user_t)); //overwrite bank file
            goto NEXTUSER;}}
        if(sizeof(user_t)!=read(fd,&u,sizeof(user_t))){
          //close(fd);
          //for(auto it=ud.begin();it!=ud.end();it++){
          //  close(*it);}
          //if(ud>=0){
          //  close(ud);}
          //std::cerr << "ERROR loading bank "<<bank<<" (bad read)\n";
          LOG("ERROR loading bank %04X (bad read)\n",bank);
          failed=true;}
        NEXTUSER:;
        weight+=u.weight;
        //SHA256_Update(&sha256,&u,sizeof(user_t));
        //FIXME, debug only !!!
        { user_t n;
          memcpy(&n,&u,sizeof(user_t));
          last_srvs_.user_csum(n,bank,user);
          if(memcmp(n.csum,u.csum,32)){
            LOG("ERROR !!!, checksum mismatch for user %04X:%08X [%08X<>%08X]\n",bank,user,
              *((uint32_t*)(n.csum)),*((uint32_t*)(u.csum)));
            failed=true;}
        }
        last_srvs_.xor4(csum,u.csum);}
      close(fd);
      for(auto it=ud.begin();it!=ud.end();it++){
        close(*it);}
      //if(ud>=0){
      //  close(ud);}
      if(last_srvs_.nodes[bank].weight!=weight){
        //std::cerr << "ERROR loading bank "<<bank<<" (bad sum:"<<last_srvs_.nodes[bank].weight<<"<>"<<weight<<")\n";
        LOG("ERROR loading bank %04X (bad sum:%016lX<>%016lX)\n",
          bank,last_srvs_.nodes[bank].weight,weight);
        failed=true;}
      //uint8_t hash[32];
      //SHA256_Final(hash,&sha256);
      if(memcmp(last_srvs_.nodes[bank].hash,csum,32)){
        //std::cerr << "ERROR loading bank "<<bank<<" (bad hash)\n";
        LOG("ERROR loading bank %04X (bad hash)\n",bank);
        failed=true;}}
      //if(ud>=0){
      //  unlink(filename);}
    if(failed){
      return(0);}
    return(1);
  }

  void load_banks()
  {
    //create missing bank messages
    uint16_t end=last_srvs_.nodes.size();
    missing_.lock();
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
      missing_msgs_[put_msg->hash.num]=put_msg;
      fillknown(put_msg);
      uint16_t peer=put_msg->request();
      if(peer){
        //std::cerr << "REQUESTING USR "<<bank<<" from "<<peer<<"\n";
        LOG("REQUESTING BANK %04X from %04X\n",bank,peer);
        deliver(put_msg,peer);}}
    while(missing_msgs_.size()){
      //do new requests here
      missing_.unlock();
      std::cerr << "WAITING for banks\n";
      boost::this_thread::sleep(boost::posix_time::seconds(2)); //yes, yes, use futur/promise instead
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
  { uint32_t now=time(NULL);
    auto block=headers.begin();
    now-=now%BLOCKSEC;
    do_validate=1;
    threadpool.create_thread(boost::bind(&server::validator, this));
    threadpool.create_thread(boost::bind(&server::validator, this));
//FIXME, must start with a matching nowhash and load serv_
    if(srvs_.now<now){
      peer_.lock();
      uint32_t n=headers.size();
      peer_.unlock();
      for(;!n;){
        LOG("\nWAITING 1s\n");
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        peer_.lock();
        n=headers.size();
        peer_.unlock();}
      peer_.lock();
      block=headers.begin();
      peer_.unlock();
      for(;;){
        LOG("START syncing header %08X\n",block->now);
        if(srvs_.now!=block->now){
          LOG("ERROR, got strange block numbers %08X<>%08X\n",srvs_.now,block->now);
          exit(-1);} //FIXME, prevent this
        //block->load_signatures(); //TODO should go through signatures and update vok, vno
        block->header_put(); //FIXME will loose relation to signatures, change signature filename to fix this
        if(!block->load_msglist(missing_msgs_,opts_.svid)){
          LOG("LOAD messages from peers\n");
          //request list of transactions from peers
          peer_.lock(); // consider changing this to missing_lock
          get_msglist=srvs_.now;
          peer_.unlock();
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
                //std::cerr << "REQUESTING MSL from "<<svid<<"\n";
                LOG("REQUESTING MSL from %04X\n",svid);
                deliver(put_msg,svid);}}
            boost::this_thread::sleep(boost::posix_time::milliseconds(50));}
          srvs_.msg=block->msg; //check
          srvs_.msg_put(missing_msgs_);}
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
        dbl_.lock();
        dbl_msgs_.clear();
        dbl_.unlock();
        ldc_.lock();
        ldc_msgs_.clear();
        ldc_.unlock();
        std::set<uint16_t> update;
        missing_.lock();
        message_queue commit_msgs;
        for(auto it=missing_msgs_.begin();it!=missing_msgs_.end();){
          missing_.unlock();
          update.insert(it->second->svid);
          auto jt=it++;
          ldc_.lock();
          ldc_msgs_[jt->first]=jt->second;
          ldc_.unlock();
          if(jt->second->hash.dat[1]==MSGTYPE_DBL){
            dbl_.lock();
            dbl_msgs_[jt->first]=jt->second;
            dbl_.unlock();}
          else{
            txs_.lock();
            txs_msgs_[jt->first]=jt->second;
            commit_msgs.push_back(jt->second);
            txs_.unlock();}
          if(jt->second->load(0)){ // will be unloaded by the validator
            if(!jt->second->sigh_check()){
              jt->second->read_head(); //to get 'now'
              LOG("LOADING TXS %04X:%08X from path:%08X\n",
                jt->second->svid,jt->second->msid,jt->second->path);
              check_.lock();
              check_msgs_.push_back(jt->second); // send to validator
              check_.unlock();
              missing_msgs_erase(jt->second);
              continue;}
            LOG("LOADING TXS %04X:%08X from path:%08X failed\n",
              jt->second->svid,jt->second->msid,jt->second->path);
            jt->second->len=message::header_length;}
          fillknown(jt->second);
          uint16_t svid=jt->second->request(); //FIXME, maybe request only if this is the next needed message, need to have serv_ ... ready for this check :-/
          if(svid){
            if(srvs_.nodes[jt->second->svid].msid==jt->second->msid-1){ // do not request if previous message not processed
              LOG("REQUESTING TXS %04X:%08X from %04X\n",jt->second->svid,jt->second->msid,svid);
              deliver(jt->second,svid);}
            else{
              LOG("POSTPONING TXS %04X:%08X\n",jt->second->svid,jt->second->msid);}}
          missing_.lock();}
        missing_.unlock();
        //wait for all messages to be processed by the validators
        ldc_.lock();
        while(ldc_msgs_.size()){
          ldc_.unlock();
//boost::this_thread::sleep(boost::posix_time::seconds(1));
//blk_.lock();
//LOG("LEFT %d messages\n",(int)ldc_msgs_.size());
//for(auto it=ldc_msgs_.begin();it!=ldc_msgs_.end();it++){
//  LOG("MSG: %04X:%08X\n",it->second->svid,it->second->msid);}
//blk_.unlock();
          boost::this_thread::sleep(boost::posix_time::milliseconds(50)); //yes, yes, use futur/promise instead
          ldc_.lock();}
        ldc_.unlock();
        //LOG("TXSHASH: %08X\n",*((uint32_t*)srvs_.msghash));
        std::cerr << "COMMIT deposits\n";
        commit_block(update); // process bkn and get transactions
        commit_dividends(update);
        commit_deposit(update);
        commit_bankfee();
        std::cerr << "UPDATE accounts\n";
        for(auto it=update.begin();it!=update.end();it++){
          assert(*it<srvs_.nodes.size());
          if(!srvs_.check_nodehash(*it)){ //FIXME, remove this later !, this is checked during download.
            LOG("FATAL ERROR, failed to check the hash of bank %04X at block %08X\n",*it,srvs_.now);
            exit(-1);}}
        //finish block
        srvs_.finish(); //FIXME, add locking
        if(memcmp(srvs_.nowhash,block->nowhash,SHA256_DIGEST_LENGTH)){
          //std::cerr<<"ERROR, failed to arrive at correct hash at block "<<srvs_.now<<", fatal\n";
          LOG("ERROR, failed to arrive at correct hash at block %08X, fatal\n",srvs_.now);
          exit(-1);}
        last_srvs_=srvs_; // consider not making copies of nodes
        memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
        period_start=srvs_.nextblock();
        //FIXME should be a separate thread
        LOG("UPDATE LOG\n");
        ofip_update_block(period_start,0,commit_msgs,srvs_.div);
        LOG("PROCESS LOG\n");
        ofip_process_log(srvs_.now-BLOCKSEC);
        LOG("WRITE NEW MSID\n");
        writemsid();
        now=time(NULL);
        now-=now%BLOCKSEC;
        if(srvs_.now>=now){
          break;}
        peer_.lock();
        for(block++;block==headers.end();block++){ // wait for peers to load more blocks
          block--;
          peer_.unlock();
          LOG("WAITING at block end (headers:%d) (srvs_.now:%08X;now:%08X) \n",
            (int)headers.size(),srvs_.now,now);
          //FIXME, insecure !!! better to ask more peers (disconnect if needed) and wait for block with enough votes
          get_more_headers(block->now+BLOCKSEC);
          boost::this_thread::sleep(boost::posix_time::seconds(2));
          peer_.lock();}
        peer_.unlock();}}
    //TODO, add nodes if needed
    vip_max=srvs_.update_vip();
    txs_.lock();
    txs_msgs_.clear();
    txs_.unlock();
    dbl_.lock();
    dbl_msgs_.clear();
    dbl_.unlock();
    //FIXME, inform peers about sync status
    peer_.lock();
    do_sync=0;
    headers.clear();
    peer_.unlock();
    message_ptr put_msg(new message());
    put_msg->data[0]=MSGTYPE_SOK;
    memcpy(put_msg->data+1,&srvs_.now,4);
    deliver(put_msg);
  }

  void put_msglist(uint32_t now,std::map<uint64_t,message_ptr>& map)
  { missing_.lock(); // consider changing this to missing_lock
    if(get_msglist!=now){
      missing_.unlock();
      return;}
    missing_msgs_.swap(map);
    get_msglist=0;
    missing_.unlock();
    return;
  }

  void add_headers(std::vector<servers>& peer_headers)
  { if(!do_sync){
      return;}
    peer_.lock();
    if(!headers.size()){
      headers.insert(headers.end(),peer_headers.begin(),peer_headers.end());
      peer_.unlock();
      return;}
    auto it=peer_headers.begin();
    for(;it!=peer_headers.end() && it->now<=headers.back().now;it++){}
    if(headers.back().now!=peer_headers.begin()->now-BLOCKSEC){
      std::cerr<<"ERROR, headers misaligned\n"; //should never happen
      peer_.unlock();
      return;}
    headers.insert(headers.end(),it,peer_headers.end());
    peer_.unlock();
  }

  int fast_sync(bool done,header_t& head,node_t* nods,svsi_t* svsi)
  { static uint32_t last=0;
    for(;;){
      uint32_t now=time(NULL);
      peer_.lock();
      if(!do_fast){
        peer_.unlock();
        return(-1);}
      if(done){ // peer should now overwrite servers with current data
	std::cerr<<"SYNC overwrite\n";
        last_srvs_.overwrite(head,nods);
	std::cerr<<"SYNC mkdir\n";
        last_srvs_.blockdir();
	std::cerr<<"SYNC put\n";
	last_srvs_.put();
	std::cerr<<"SYNC put signatures\n";
	last_srvs_.put_signatures(head,svsi);

        //FIXME, do this after loading banks
	std::cerr<<"SYNC copy\n";
        srvs_=last_srvs_; //FIXME, create a copy function
        memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
	std::cerr<<"SYNC nextblock\n";
        period_start=srvs_.nextblock();
	std::cerr<<"SYNC update vip\n";
        vip_max=srvs_.update_vip(); //based on final weights
        do_fast=0;
        peer_.unlock();
        return(1);}
      if(last<now-SYNC_WAIT){
        last=now;
        peer_.unlock();
        return(1);}
      peer_.unlock();
      boost::this_thread::sleep(boost::posix_time::seconds(1));}
      return 0;
  }

  uint32_t readmsid()
  { FILE* fp=fopen("msid.txt","r");
    if(fp==NULL){
      msid_=0;
      return(0);}
    uint32_t path;
    uint32_t svid;
    fscanf(fp,"%X %X %X",&msid_,&path,&svid);
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
    fprintf(fp,"%08X %08X %04X\n",msid_,last_srvs_.now,opts_.svid);
    fclose(fp);
  }

  //move this to message
  void message_msha(std::map<uint16_t,message_ptr>& map)
  { svid_msha.clear();
    for(std::map<uint16_t,message_ptr>::iterator it=map.begin();it!=map.end();++it){
      msidhash_t msha;
      msha.msid=it->second->msid;
      memcpy(msha.sigh,it->second->sigh,sizeof(hash_t));
      svid_msha[it->first]=msha;}
  }

  //move this to message
  void message_shash(uint8_t* mhash,std::map<uint16_t,message_ptr>& map)
  { SHA256_CTX sha256;
    SHA256_Init(&sha256);
    for(std::map<uint16_t,message_ptr>::iterator it=map.begin();it!=map.end();++it){
      char sigh[2*SHA256_DIGEST_LENGTH];
      ed25519_key2text(sigh,it->second->sigh,SHA256_DIGEST_LENGTH);
      LOG("SHASH: %04X:%08X %.*s\n",it->first,it->second->msid,2*SHA256_DIGEST_LENGTH,sigh);
      // do not hash messages from ds_server
      SHA256_Update(&sha256,it->second->sigh,4*sizeof(uint64_t));}
    SHA256_Final(mhash, &sha256); // std::cerr << "message_shash\n";
  }

  void count_votes(uint32_t now,hash_s& cand) // cand_.locked()
  { candidate_ptr num1=NULL;
    candidate_ptr num2=NULL;
    uint64_t votes_counted=0;
    hash_s best;
    cand_.lock();
    for(auto it=candidates_.begin();it!=candidates_.end();it++){ // cand_ is locked
      if(num1==NULL || it->second->score>num1->score){
        num2=num1;
        memcpy(&best,&it->first,sizeof(hash_s));
        num1=it->second;}
      else if(num2==NULL || it->second->score>num2->score){
        num2=it->second;}
      votes_counted+=it->second->score;}
    cand_.unlock();
    if(num1==NULL){
      if(do_vote && now>srvs_.now+BLOCKSEC+(do_vote-1)*VOTE_DELAY){
        std::cerr << "CANDIDATE proposing\n";
        write_candidate(cand);}
      return;}
    if(do_block<2 && (
        (num1->score>(num2!=NULL?num2->score:0)+(votes_max-votes_counted))||
        (now>srvs_.now+BLOCKSEC+MAX_ELEWAIT))){
      uint64_t x=(num2!=NULL?num2->score:0);
      //std::cerr << "BLOCK elected: " << num1->score << " second:" << x << " max:" << votes_max << " counted:" << votes_counted << "\n";
      if(now>srvs_.now+BLOCKSEC+MAX_ELEWAIT){
        LOG("CANDIDATE SELECTED:%016lX second:%016lX max:%016lX counted:%016lX BECAUSE OF TIMEOUT!!!\n",
          num1->score,x,votes_max,votes_counted);}
      else{
        LOG("CANDIDATE ELECTED:%016lX second:%016lX max:%016lX counted:%016lX\n",
          num1->score,x,votes_max,votes_counted);}
      do_block=2;
      winner=num1;
      if(winner->failed_peer){
        std::cerr << "BAD CANDIDATE elected :-( must resync :-( \n"; // FIXME, do not exit, initiate sync
        exit(-1);}}
    if(do_block==2 && winner->elected_accept()){
      std::cerr << "CANDIDATE winner accepted\n";
      do_block=3;
      if(do_vote){
        write_candidate(best);}
      return;}
    if(do_block==2){
      LOG("ELECTION: %s\n",winner->print_missing(&srvs_));}
      //std::string list;
      //cand_.lock();
      //for(auto it=winner->waiting_server.begin();it!=winner->waiting_server.end();it++){
      //   list+=" ";
      //   list+=std::to_string(*it);}
      //cand_.unlock();
      //LOG("CANDIDATE missing %s\n",list.c_str());
    if(do_vote && num1->accept() && num1->peers.size()>1){
      std::cerr << "CANDIDATE proposal accepted\n";
      write_candidate(best);
      return;}
    if(do_vote && now>srvs_.now+BLOCKSEC+(do_vote-1)*VOTE_DELAY){
      std::cerr << "CANDIDATE proposing\n";
      write_candidate(cand);}
  }

  void add_electors(header_t& head,svsi_t* peer_svsi)
  { hash_t empty;
    for(int i=0;i<head.vok;i++){
      uint8_t* data=(uint8_t*)&peer_svsi[i];
      uint16_t svid;
      memcpy(&svid,data,2);
      message_ptr msg(new message(MSGTYPE_BLK,(uint8_t*)&head,sizeof(header_t),svid,head.now,data+2,NULL,empty));
      msg->status=MSGSTAT_VAL;
      msg->svid=svid;
      msg->peer=svid; //to allow insertion
      msg->msid=head.now;
      blk_insert(msg);}
  }
      
  void prepare_poll() // select CANDIDATE_MAX candidates and VOTES_MAX electors
  { 
    cand_.lock();
    votes_max=0.0;
    do_vote=0;
    //FIXME, this should be moved to servers.hpp
    std::set<uint16_t> svid_rset;
    std::vector<uint16_t> svid_rank;
    //for(auto it=last_svid_msgs.begin();it!=last_svid_msgs.end();++it){ // this is too unstable
    //  if(srvs_.nodes[it->second->svid].status & SERVER_DBL){
    //    continue;}
    //  LOG("ELECTOR accepted:%04X (msg)\n",(it->second->svid));
    //  svid_rset.insert(it->second->svid);}
    for(auto it=blk_msgs_.begin();it!=blk_msgs_.end();++it){ //add also nodes with blk_msgs_
      if((it->second->msid!=srvs_.now-BLOCKSEC) || 
         !(it->second->status & MSGSTAT_VAL) ||
         (srvs_.nodes[it->second->svid].status & SERVER_DBL)){
        int a=(int)(!(it->second->status & MSGSTAT_VAL));
        int b=(int)(srvs_.nodes[it->second->svid].status & SERVER_DBL);
        LOG("ELECTOR blk ignor %04X (%08X<>%08X||%d||%d)\n",
          it->second->svid,it->second->msid,srvs_.now-BLOCKSEC,a,b);
        continue;}
      LOG("ELECTOR accepted:%04X (blk)\n",(it->second->svid));
      svid_rset.insert(it->second->svid);}
    if(!svid_rset.size()){
      LOG("ERROR, no valid server for this block :-(\n");}
    else{
      for(auto sv : svid_rset){
        svid_rank.push_back(sv);}
      std::sort(svid_rank.begin(),svid_rank.end(),[this](const uint16_t& i,const uint16_t& j){return(this->srvs_.nodes[i].weight>this->srvs_.nodes[j].weight);});} //fuck, lambda :-/
    //TODO, save this list
    for(uint32_t j=0;j<VOTES_MAX && j<svid_rank.size();j++){
      if(svid_rank[j]==opts_.svid){
        do_vote=1+j;}
      LOG("ELECTOR[%d]=%016lX\n",svid_rank[j],srvs_.nodes[svid_rank[j]].weight);
      electors[svid_rank[j]]=srvs_.nodes[svid_rank[j]].weight;
      votes_max+=srvs_.nodes[svid_rank[j]].weight;}
    winner=NULL;
    LOG("ELECTOR max:%016lX\n",votes_max);
    cand_.unlock();
  }

  message_ptr message_svidmsid(uint16_t svid,uint32_t msid)
  { 
    union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=0; // hash
    h.dat[1]=0; // message type
    memcpy(h.dat+2,&msid,4);
    memcpy(h.dat+6,&svid,2);
    LOG("HASH find:%016lX (%04X:%08X) %d:%d\n",h.num,svid,msid,svid,msid);
    txs_.lock();
    auto mi=txs_msgs_.lower_bound(h.num);
    while(mi!=txs_msgs_.end() && mi->second->svid==svid && mi->second->msid==msid){
      if(mi->second->status==MSGSTAT_VAL){
        txs_.unlock();
        return mi->second;}
      mi++;}
    txs_.unlock();
    return NULL;
  }

  message_ptr message_find(message_ptr msg,uint16_t svid)
  { LOG("HASH find:%016lX (%04X%08X) %d:%d\n",msg->hash.num,msg->svid,msg->msid,msg->svid,msg->msid);
    assert(msg->data!=NULL);
    if(msg->data[0]==MSGTYPE_GET){
      txs_.lock();
      std::map<uint64_t,message_ptr>::iterator it=txs_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=txs_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64 && msg->hash.dat[0]==it->second->hashval(svid)){ //data[4+(svid%64)]
          txs_.unlock();
          return it->second;}
        it++;}
      txs_.unlock();
      return NULL;}
    if(msg->data[0]==MSGTYPE_CNG){
      cnd_.lock();
      std::map<uint64_t,message_ptr>::iterator it=cnd_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=cnd_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64 && msg->hash.dat[0]==it->second->hashval(svid)){ //data[4+(svid%64)]
          cnd_.unlock();
          return it->second;}
        it++;}
      cnd_.unlock();
LOG("HASH find failed, CND db:\n");
for(auto me=cnd_msgs_.begin();me!=cnd_msgs_.end();me++){ LOG("HASH have: %016lX (%02X)\n",me->first,me->second->hashval(svid));} //data[4+(svid%64)]
      return NULL;}
    if(msg->data[0]==MSGTYPE_BLG){
      blk_.lock();
      std::map<uint64_t,message_ptr>::iterator it=blk_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=blk_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64 && msg->hash.dat[0]==it->second->hashval(svid)){ //data[4+(svid%64)]
          blk_.unlock();
          return it->second;}
        it++;}
      blk_.unlock();
      return NULL;}
    if(msg->data[0]==MSGTYPE_DBG){
      dbl_.lock();
      std::map<uint64_t,message_ptr>::iterator it=dbl_msgs_.lower_bound(msg->hash.num & 0xFFFFFFFFFFFFFF00L);
      while(it!=dbl_msgs_.end() && ((it->first & 0xFFFFFFFFFFFFFF00L)==(msg->hash.num & 0xFFFFFFFFFFFFFF00L))){
        if(it->second->len>4+64){
          dbl_.unlock();
          return it->second;}
        it++;}
      dbl_.unlock();
      return NULL;}
    LOG("UNKNOWN hashtype:%d %02X\n",(uint32_t)msg->data[0],(uint32_t)msg->data[0]);
    return NULL;
  }

  void double_spend(message_ptr msg)
  {
    std::cerr << "WARNING, double spend maybe not yet fully implemented\n";
    svid_.lock();
    srvs_.nodes[msg->svid].status|=SERVER_DBL;
    srvs_.nodes[msg->svid].msid=msg->msid; //FIXME, this should be maybe(!) msid from last block + 1
    svid_msgs_[msg->svid]=msg;
    svid_.unlock();
    if(!do_sync){
      update(msg);}
    // undo transactions later
  }

  void create_double_spend_proof(message_ptr msg1,message_ptr msg2)
  { try{
      assert(!do_sync); // should never happen, should never get same msid from same server in a msg_list
      uint32_t len=4+msg1->len+msg2->len;
      assert(msg1->svid==msg2->svid);
      assert(msg1->msid==msg2->msid);
      if(msg1->svid==opts_.svid){
        std::cerr << "FATAL, created own double spend !!!\n";
        exit(-1);}
      message_ptr dbl_msg(new message(len));
      msg1->load(0);
      msg2->load(0);
      dbl_msg->data[0]=MSGTYPE_DBL;
      memcpy(dbl_msg->data+1,&len,3);
//FIXME, include previous hash in this double spend proof
// preh !!!
      memcpy(dbl_msg->data+4,msg1->data,msg1->len);
      memcpy(dbl_msg->data+4+msg1->len,msg2->data,msg2->len);
      dbl_msg->msid=0xffffffff;
      dbl_msg->svid=msg1->svid;
      dbl_msg->now=time(NULL);
      dbl_msg->peer=opts_.svid;
      dbl_msg->hash.num=dbl_msg->dohash();
      dbl_msg->null_signature(); //FIXME, set this to last hash from last block
      msg1->unload(0);
      msg2->unload(0);
      dbl_.lock();
      dbl_msgs_[dbl_msg->hash.num]=dbl_msg;
      dbl_.unlock();
      double_spend(dbl_msg);}
    catch(std::out_of_range& e){
      return;}
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
    std::cerr << "ERROR, getting unexpected message type :-( \n";
    return(-1);
  }

  int dbl_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  {
    assert(msg->hash.dat[1]==MSGTYPE_DBL);
    dbl_.lock(); // maybe no lock needed
    LOG("HASH insert:%016lX (DBL)\n",msg->hash.num);
    auto it=dbl_msgs_.find(msg->hash.num);
    if(it!=dbl_msgs_.end()){
      message_ptr osg=it->second;
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
        if(do_sync && memcmp(osg->sigh,msg->sigh,SHA256_DIGEST_LENGTH)){
          dbl_.unlock();
          std::cerr << "ERROR, getting message with wrong signature hash\n";
          return(0);}
        osg->update(msg);
        osg->path=srvs_.now;
        dbl_.unlock();
        missing_msgs_erase(msg);
        if(!osg->save()){
          std::cerr << "ERROR, message save failed, abort server\n";
          exit(-1);}
        double_spend(osg);
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
    if(msg->svid==opts_.svid){ // own message
      dbl_msgs_[msg->hash.num]=msg;
      dbl_.unlock();
      assert(msg->peer==msg->svid);
      std::cerr << "DEBUG, storing own dbl message :-( [???]\n";
      msg->save();
      return(1);}
    dbl_.unlock();
    std::cerr << "ERROR, getting unexpected dbl message\n";
    return(-1);
  }

  int cnd_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  { assert(msg->hash.dat[1]==MSGTYPE_CND);
    cnd_.lock();
    LOG("HASH insert:%016lX (CND) [len:%d]\n",msg->hash.num,msg->len);
    std::map<uint64_t,message_ptr>::iterator it=cnd_msgs_.find(msg->hash.num);
    if(it!=cnd_msgs_.end()){
      message_ptr osg=it->second;
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
	message_ptr pre=NULL,nxt=NULL;
        osg->update(msg);
        osg->path=osg->msid; // this is the block time!!!
        cnd_.unlock();
        missing_msgs_erase(msg);
        if(!osg->save()){ //FIXME, do not exit !!! return fail
          std::cerr << "ERROR, message save failed, abort server\n";
          return(-1);}
        if(it!=cnd_msgs_.begin()){
          pre=(--it)->second;
          it++;}
        if((++it)!=cnd_msgs_.end()){
          nxt=it->second;}
        if(pre!=NULL && pre->len>message::header_length && (pre->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
          create_double_spend_proof(pre,osg); // should copy messages from this server to ds_msgs_
          return(1);}
        if(nxt!=NULL && nxt->len>message::header_length && (nxt->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
          create_double_spend_proof(nxt,osg); // should copy messages from this server to ds_msgs_
          return(1);}
        std::cerr << "DEBUG, storing cnd message\n";
        cnd_validate(osg);
        return(1);}
      else{ // update info about peer inventory
        cnd_.unlock();
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    if(msg->len==message::header_length){
      cnd_msgs_[msg->hash.num]=msg;
      cnd_.unlock();
      missing_msgs_insert(msg);
      return(1);}
    if(msg->svid==opts_.svid){ // own message
      cnd_msgs_[msg->hash.num]=msg;
      msg->path=msg->msid; // this is the block time!!!
      cnd_.unlock();
      assert(msg->peer==msg->svid);
      std::cerr << "DEBUG, storing own cnd message\n";
      msg->save();
      cnd_validate(msg);
      return(1);}
    cnd_.unlock();
    std::cerr << "ERROR, getting unexpected cnd message\n";
    return(-1);
  }

  int blk_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  { assert(msg->hash.dat[1]==MSGTYPE_BLK);
    blk_.lock();
    LOG("HASH insert:%016lX (BLK)\n",msg->hash.num);
    std::map<uint64_t,message_ptr>::iterator it=blk_msgs_.find(msg->hash.num);
    if(it!=blk_msgs_.end()){
      message_ptr osg=it->second;
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
	message_ptr pre=NULL,nxt=NULL;
        osg->update(msg);
        osg->path=osg->msid; // this is the block time!!!
        blk_.unlock();
        missing_msgs_erase(msg);
        if(!osg->save()){ //FIXME, save where ???, check legal time !!!
          std::cerr << "ERROR, message save failed, abort server\n";
          return(-1);}
        if(it!=blk_msgs_.begin()){
          pre=(--it)->second;
          it++;}
        if((++it)!=blk_msgs_.end()){
          nxt=it->second;}
        if(pre!=NULL && pre!=it->second && pre->len>message::header_length && (pre->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
          create_double_spend_proof(pre,osg); // should copy messages from this server to ds_msgs_
          return(1);}
        if(nxt!=NULL && nxt!=it->second && nxt->len>message::header_length && (nxt->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
          create_double_spend_proof(nxt,osg); // should copy messages from this server to ds_msgs_
          return(1);}
        std::cerr << "DEBUG, storing blk message\n";
        blk_validate(osg);
        return(1);}
      else{ // update info about peer inventory
        blk_.unlock();
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    if(msg->len==message::header_length){
      blk_msgs_[msg->hash.num]=msg; //insert block message
      blk_.unlock();
      missing_msgs_insert(msg);
      return(1);}
    if(msg->svid==opts_.svid){ // own message
      blk_msgs_[msg->hash.num]=msg;
      msg->path=msg->msid; // this is the block time!!!
      blk_.unlock();
      assert(msg->peer==msg->svid);
      std::cerr << "DEBUG, storing own blk message\n";
      msg->save(); //FIXME, time !!!
      blk_validate(msg);
      return(1);}
    if(msg->svid==msg->peer){ // peers message
      blk_msgs_[msg->hash.num]=msg;
      msg->path=msg->msid; // this is the block time!!!
      blk_.unlock();
      std::cerr << "DEBUG, storing peer's blk message\n";
      msg->save(); //FIXME, time !!!
      blk_validate(msg);
      return(1);}
    blk_.unlock();
    std::cerr << "ERROR, getting unexpected blk message\n";
    return(-1);
  }

  int txs_insert(message_ptr msg) // WARNING !!! it deletes old message data if len==message::header_length
  { assert(msg->hash.dat[1]==MSGTYPE_MSG);
    txs_.lock(); // maybe no lock needed
    //LOG("HASH insert:%016lX (TXS) [len:%d]\n",msg->hash.num,msg->len);
    std::map<uint64_t,message_ptr>::iterator it=txs_msgs_.find(msg->hash.num);
    if(it!=txs_msgs_.end()){
      message_ptr osg=it->second;
      // overwrite message status with the current one
      // not needed any more msg->status=osg->status; // for peer.hpp to check if message is already validated
      if(msg->len>message::header_length && osg->len==message::header_length){ // insert full message
        if(do_sync && memcmp(osg->sigh,msg->sigh,SHA256_DIGEST_LENGTH)){
          txs_.unlock();
          LOG("HASH insert:%016lX (TXS) [len:%d] WRONG SIGNATURE HASH!\n",msg->hash.num,msg->len);
          return(0);}
        osg->update(msg);
        osg->path=srvs_.now;
        txs_.unlock();
        missing_msgs_erase(msg);
        if(!osg->save()){ //FIXME, change path
          LOG("HASH insert:%016lX (TXS) [len:%d] SAVE FAILED, ABORT!\n",osg->hash.num,osg->len);
          exit(-1);}
        osg->unload(0);
        // process double spend
        //if(osg->hash.dat[1]==MSGTYPE_DBL){ // double spend proof
        //  double_spend(osg);
        //  return(1);}
        // check for double spend
        if(!do_sync){
          message_ptr pre=NULL,nxt=NULL; //probably not needed when syncing
          if(it!=txs_msgs_.begin()){
            pre=(--it)->second;
            it++;}
          if((++it)!=txs_msgs_.end()){
            nxt=it->second;}
          if(pre!=NULL && pre!=it->second && pre->len>message::header_length && (pre->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
            LOG("HASH insert:%016lX (TXS) [len:%d] DOUBLE SPEND!\n",osg->hash.num,osg->len);
            create_double_spend_proof(pre,osg); // should copy messages from this server to ds_msgs_
            return(1);}
          if(nxt!=NULL && nxt!=it->second && nxt->len>message::header_length && (nxt->hash.num&0xFFFFFFFFFFFF0000L)==(osg->hash.num&0xFFFFFFFFFFFF0000L)){
            LOG("HASH insert:%016lX (TXS) [len:%d] DOUBLE SPEND!\n",osg->hash.num,osg->len);
            create_double_spend_proof(nxt,osg); // should copy messages from this server to ds_msgs_
            return(1);}
          if(osg->now>=srvs_.now+BLOCKSEC){
            LOG("HASH insert:%016lX (TXS) [len:%d] delay to %08X/ ???\n",osg->hash.num,osg->len,osg->now);
            wait_.lock();
            wait_msgs_.push_back(osg);
            wait_.unlock();
            return(1);}}//FIXME, process wait messages later
        // process ordinary messages
        LOG("HASH insert:%016lX (TXS) [len:%d] queued\n",osg->hash.num,osg->len);
        check_.lock();
        check_msgs_.push_back(osg);
        check_.unlock();
        return(1);}
      else{ // update info about peer inventory
        txs_.unlock();
        LOG("HASH insert:%016lX (TXS) [len:%d] ignored\n",msg->hash.num,msg->len);
        osg->know_insert(msg->peer);
        return(0);}} // RETURN, message known info
    else{
      if(msg->len==message::header_length){
        txs_msgs_[msg->hash.num]=msg;
        txs_.unlock();
        LOG("HASH insert:%016lX (TXS) [len:%d] set as missing\n",msg->hash.num,msg->len);
        missing_msgs_insert(msg);
        return(1);}
      if(msg->svid==opts_.svid){ // own message
        txs_msgs_[msg->hash.num]=msg;
	//if(msg->path && msg->path!=srvs_.now){ //move message
        //  LOG("HASH insert:%016lX (TXS) [len:%d] path mismatch (%08X<>%08X)\n",
        //    msg->hash.num,msg->len,msg->path,srvs_.now);
        //  exit(-1);}
        //assert(msg->path==srvs_.now || msg->path==srvs_.now+BLOCKSEC);
        msg->path=srvs_.now; // all messages must be in current block
        txs_.unlock();
        LOG("HASH insert:%016lX (TXS) [len:%d] store as own\n",msg->hash.num,msg->len);
	//saved before insertion
        if(!msg->save()){
          LOG("ERROR, failed to save own message %08X, fatal\n",msg->msid);
          exit(-1);}
        msg->unload(0);
        if(msg->now>=srvs_.now+BLOCKSEC){
          LOG("\nHASH insert:%016lX (TXS) [len:%d] delay to %08X/ OWN MESSAGE !!!\n\n", //FIXME, fatal in start !!!
            msg->hash.num,msg->len,msg->now);
          wait_.lock();
          wait_msgs_.push_back(msg);
          wait_.unlock();}
        else{
          check_.lock();
          check_msgs_.push_back(msg); // running though validator
          check_.unlock();}
	assert(msg->peer==msg->svid);
        return(1);}
      txs_.unlock();
      LOG("HASH insert:%016lX (TXS) [len:%d] UNEXPECTED!\n",msg->hash.num,msg->len);
      return(-1);}
  }

  void cnd_validate(message_ptr msg)
  {
    std::cerr << "CANDIDATE test\n";
    msg->status=MSGSTAT_VAL; // all can vote
    cand_.lock();
    if(electors.find(msg->svid)==electors.end()){ //FIXME, electors should have assigned rank when building poll
      //std::cerr << "BAD ELECTOR "<< msg->svid <<" :-( \n";
      LOG("BAD ELECTOR %04X\n",msg->svid);
      cand_.unlock();
      return;}
    if(!electors[msg->svid]){
      cand_.unlock();
      return;}
    hash_s cand; //TODO
    assert(msg->data!=NULL);
    memcpy(cand.hash,msg->data+message::data_offset,sizeof(hash_t));
    auto it=candidates_.find(cand);
    if(it==candidates_.end()){
      std::cerr << "BAD CANDIDATE :-( \n";
      cand_.unlock();
      return;}
    it->second->score+=electors[msg->svid]; // update sum of weighted votes
    //std::cerr << "CANDIDATE score: "<< it->second->score <<"(+"<< electors[msg->svid] <<")\n";
    LOG("CANDIDATE score:%016lX (added:%016lX)\n",it->second->score,electors[msg->svid]);
    electors[msg->svid]=0;
    cand_.unlock();
    update(msg); // update others
  }

  void blk_validate(message_ptr msg)
  {

//FIXME, check what block are we dealing with !!!

    std::cerr << "BLOCK test\n";
    if(msg->msid!=last_srvs_.now){
      LOG("BLOCK bad msid:%08x block:%08x\n",msg->msid,last_srvs_.now);
      return;}
    uint32_t vip=last_srvs_.nodes[msg->svid].status & SERVER_VIP;
    if(!vip){
      LOG("BLOCK ignore non-vip vote msid:%08x svid:%04x\n",msg->msid,(uint32_t)msg->svid);
      return;}
    msg->status=MSGSTAT_VAL;
    assert(msg->data!=NULL);
    header_t* h=(header_t*)(msg->data+4+64+10); 
    bool no=memcmp(h->nowhash,last_srvs_.nowhash,sizeof(hash_t));
    blk_.lock();
    last_srvs_.save_signature(msg->svid,msg->data+4,!no);
    blk_.unlock();
    LOG("BLOCK: yes:%d no:%d max:%d\n",last_srvs_.vok,last_srvs_.vno,vip_max);
    update(msg); // update others if this is a VIP message, my message was sent already, but second check will not harm
    if(last_srvs_.vno>vip_max/2){
      std::cerr << "BAD BLOCK consensus :-( must resync :-( \n"; // FIXME, do not exit, initiate sync
      exit(-1);}
    if(no){
      if(msg->peer==msg->svid){
        LOG("\n\nBLOCK differs, disconnect from %04X!\n\n\n",msg->svid);
        disconnect(msg->svid);}}
  }

  void missing_sent_remove(uint16_t svid) //TODO change name to missing_know_send_remove()
  { missing_.lock();
    for(auto mi=missing_msgs_.begin();mi!=missing_msgs_.end();mi++){
      mi->second->mtx_.lock();
      mi->second->know.erase(svid);
      mi->second->sent.erase(svid);
      mi->second->mtx_.unlock();}
    missing_.unlock();
  }

  void validator(void)
  {
    while(do_validate){
      //LOG("VALIDATOR\n");
      check_.lock(); //TODO this should be a lock for check_msgs_ only maybe
      if(check_msgs_.empty()){
        check_.unlock();
        //look for missing messages
        //std::cerr << "VALIDATOR housekeeping\n";
        uint32_t now=time(NULL);
	message_queue tmp_msgs_;
	missing_.lock();
	for(auto mj=missing_msgs_.begin();mj!=missing_msgs_.end();){
          //FIXME, request _BLK messages if BLOCK ready 
          auto mi=mj++;
          if(mi->second->hash.dat[1]==MSGTYPE_BLK){ //FIXME, consider checking mi->first
            if(mi->second->msid<last_srvs_.now-BLOCKSEC){
              missing_msgs_.erase(mi);
              continue;}
            if(now>mi->second->got+MAX_MSGWAIT && mi->second->msid<=last_srvs_.now){
              tmp_msgs_.push_back(mi->second);}}
          else{
            if(mi->second->msid<srvs_.nodes[mi->second->svid].msid){
              missing_msgs_.erase(mi);
              continue;}
            if(mi->second->got<=now-MAX_MSGWAIT && srvs_.nodes[mi->second->svid].msid==mi->second->msid-1){
              tmp_msgs_.push_back(mi->second);}}}
	missing_.unlock();
        for(auto re=tmp_msgs_.begin();re!=tmp_msgs_.end();re++){
	  uint16_t svid=(*re)->request();
          if(svid){
            assert((*re)->data!=NULL);
            LOG("HASH request:%016lX [%016lX] (%04X) %d:%d\n",(*re)->hash.num,*((uint64_t*)(*re)->data),svid,(*re)->svid,(*re)->msid); // could be bad allignment
            //std::cerr << "REQUESTING MESSAGE from "<<svid<<" ("<<(*re)->svid<<":"<<(*re)->msid<<")\n";
            LOG("REQUESTING MESSAGE %04X:%08X from %04X\n",(*re)->svid,(*re)->msid,svid);
            deliver((*re),svid);}}
        //checking waiting messages
        tmp_msgs_.clear();
        wait_.lock();
	for(auto wa=wait_msgs_.begin();wa!=wait_msgs_.end();){
          if((*wa)->now<srvs_.now+BLOCKSEC && srvs_.nodes[(*wa)->svid].msid==(*wa)->msid-1){
            //std::cerr << "QUEUING MESSAGE "<<(*wa)->svid<<":"<<(*wa)->msid<<"\n";
            LOG("QUEUING MESSAGE %04X:%08X\n",(*wa)->svid,(*wa)->msid);
            tmp_msgs_.push_back(*wa);
            wa=wait_msgs_.erase(wa);}
          else{
            wa++;}}
        wait_.unlock();
	check_.lock();
        check_msgs_.insert(check_msgs_.end(),tmp_msgs_.begin(),tmp_msgs_.end());
        check_.unlock();
        //TODO, check if there are no forgotten messeges in the missing_msgs_ queue
	boost::this_thread::sleep(boost::posix_time::milliseconds(100));} // adds latency but works
      else{
        message_ptr msg=check_msgs_.front();
	check_msgs_.pop_front();
        //could concider a custom lock check against opening the same usr/XXXX.dat file
        //this will be needed later if we need to provide usr/XXXX.dat file for syncing
        check_.unlock();
        if(srvs_.nodes[msg->svid].status & SERVER_DBL){ // ignore messages from DBL server
          continue;} // no update
        //if(msg->status==MSGSTAT_VAL || srvs_.nodes[msg->svid].msid>=msg->msid)
        if(srvs_.nodes[msg->svid].msid>=msg->msid){
          //std::cerr <<"WARNING ignoring validation of old message "<<msg->svid<<":"<<msg->msid<<"<="<<srvs_.nodes[msg->svid].msid<<"\n";
          LOG("WARNING ignoring validation of old message %04X:%08X (<=%08X)\n",
            msg->svid,msg->msid,srvs_.nodes[msg->svid].msid);
          continue;}
	if(srvs_.nodes[msg->svid].msid!=msg->msid-1){ //assume only 1 validator per bank
          //std::cerr <<"WARNING postponing validation of future message "<<msg->svid<<":"<<msg->msid<<"!="<<srvs_.nodes[msg->svid].msid<<"+1\n";
          LOG("WARNING postponing validation of future message %04X:%08X (!=%08X+1)\n",
            msg->svid,msg->msid,srvs_.nodes[msg->svid].msid);
          wait_.lock();
          wait_msgs_.push_back(msg);
          wait_.unlock();
          continue;}
        if(!msg->load(0)){
          LOG("ERROR, failed to load blk/%03X/%05X/%02x_%04x_%08x.msg [len:%d]\n",msg->path>>20,msg->path&0xFFFFF,(uint32_t)msg->hashtype(),msg->svid,msg->msid,msg->len);
          exit(-1);}
        bool valid=process_message(msg); //maybe ERROR should be also returned.
        if(valid){
          msg->print_text("VALID");
          msg->status=MSGSTAT_VAL;
          svid_.lock();
          node* nod=&srvs_.nodes[msg->svid];
          nod->msid=msg->msid;
          nod->mtim=msg->now;
          memcpy(nod->msha,msg->sigh,sizeof(hash_t));
          svid_msgs_[msg->svid]=msg;
          svid_.unlock();
          uint32_t now=time(NULL);
          uint64_t next=msg->hash.num+1;
	  missing_.lock();
          auto nt=missing_msgs_.lower_bound(next); //speed up next validation
          if(nt!=missing_msgs_.end()){
            if(nt->second->got>now-MAX_MSGWAIT){
              nt->second->got=now-MAX_MSGWAIT;}}
	  missing_.unlock();}
        else{
          //TODO, inform peer if peer==author;
          std::cerr<<"ERROR, have invalid message !!!\n";
          exit(-1);}
        msg->unload(0);
        if(!do_sync){
          //simulate delay, FIXME, remove after sync tests
          uint32_t seconds=(rand()%5);
          LOG("SLEEP %d after validation\n",seconds);
          boost::this_thread::sleep(boost::posix_time::seconds(seconds));
          if(valid){
            update_candidates(msg);
            update(msg);}}
        else{
          if(!valid){
            std::cerr<<"ERROR, sync failed :-( :-(\n";
            msg->print("");
            exit(-1);}
          ldc_.lock();
          ldc_msgs_.erase(msg->hash.num);
          ldc_.unlock();}}}
  }

  uint64_t make_ppi(uint32_t amsid,uint16_t abank,uint16_t bbank)
  { ppi_t ppi;
    ppi.v32[0]=amsid;
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

  bool remove_message(message_ptr msg)
  { if(msg->svid==opts_.svid){
      LOG("ERROR: trying to remove own message, MUST RESUBMIT (implement!)\n");
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
      //if(msg->svid==opts_.svid){
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

  bool undo_message(message_ptr msg)
  { if(!msg->load(0)){
      std::cerr<<"ERROR, failed to load message !!!\n";
      exit(-1);}
    assert(msg->data!=NULL);
    char* p=(char*)msg->data+4+64+10;
    std::map<uint64_t,int64_t> txs_deposit;
    std::set<uint64_t> txs_get; //set lock / withdraw
    //TODO, load message from file
    bool old_bky=false;
    //bool old_usr=false;
    while(p<(char*)msg->data+msg->len){
      //char txstype=*p;
      usertxs utxs;
      assert(*p<TXSTYPE_INF);
      if(*p==TXSTYPE_PUT){
        utxs.parse(p);
        if(utxs.bbank!=utxs.abank){
          LOG("WARNING undoing put\n");
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
            LOG("WARNING undoing mpt to: %04X:%08X<=%016lX\n",tbank,tuser,tmass);
            union {uint64_t big;uint32_t small[2];} to;
            to.small[0]=tuser; //assume big endian
            to.small[1]=tbank; //assume big endian
            txs_deposit[to.big]+=tmass;}} // will be substructed at the end of undo
	p+=utxs.size;
	continue;}
      if(*p==TXSTYPE_GET){
        utxs.parse(p);
        uint64_t ppi=make_ppi(msg->msid,msg->svid,utxs.bbank);
        txs_get.insert(ppi);
	p+=utxs.size;
        LOG("WARNING undoing get\n");
	continue;}
      if(*p==TXSTYPE_BKY && old_bky==false){ //reverse bank key change
        old_bky=true;
        memcpy(srvs_.nodes[msg->svid].pk,utxs.opkey(p),32);
        if(msg->svid==opts_.svid){
          LOG("WARNING undoing local bank key change\n");}}
      p+=utxs.get_size(p);}
    uint64_t ppi=make_ppi(msg->msid,msg->svid,msg->svid);
    blk_.lock();
    blk_usr.erase(ppi);
    blk_uok.erase(ppi);
    blk_bnk.erase(ppi);
    for(auto it=txs_get.begin();it!=txs_get.end();it++){
      blk_get.erase(*it);}
    blk_.unlock();
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
    srvs_.nodes[msg->svid].msid=msg->msid-1; //LESZEK ADDED assuming message exists
    srvs_.xor4(srvs_.nodes[msg->svid].hash,csum);
    memcpy(srvs_.nodes[msg->svid].msha,msha,SHA256_DIGEST_LENGTH);
    srvs_.nodes[msg->svid].mtim=mtim;
    //this could be a srvs_.function()
    LOG("UNDO USERS:%08X\n",users);
    if(users){
      if(srvs_.nodes[msg->svid].users!=users){
        LOG("WARNING undoing user additions (users back to:%08X)\n",users);}
      srvs_.nodes[msg->svid].users=users;}
    char filename[64];
    sprintf(filename,"usr/%04X.dat",msg->svid);
    int fd=open(filename,O_RDWR|O_CREAT,0644);
    if(fd<0){
      LOG("ERROR, failed to open bank register %04X, fatal\n",msg->svid);
      exit(-1);}
    for(auto it=undo.begin();it!=undo.end();it++){
      user_t& u=it->second;
      LOG("UNDO:%04X:%08X m:%08X t:%08X s:%04X b:%04X u:%08X l:%08X r:%08X v:%016lX\n",
        msg->svid,it->first,u.msid,u.time,u.stat,u.node,u.user,u.lpath,u.rpath,u.weight);
      lseek(fd,it->first*sizeof(user_t),SEEK_SET);
      write(fd,&it->second,sizeof(user_t));}
    close(fd);
    del_msglog(srvs_.now,msg->svid,msg->msid);
    msg->unload(0);
    return(true);
  }

  void log_broadcast(uint32_t path,char* p,int len,uint8_t* hash,uint8_t* pkey,uint32_t msid,uint32_t mpos)
  { static uint32_t lpath=0;
    static int fd=-1;
    static boost::mutex log_;
    log_.lock();
    if(path!=lpath || fd<0){
      if(fd>=0){
        close(fd);}
      lpath=path;
      char filename[64];
      sprintf(filename,"blk/%03X/%05X/bro.log",path>>20,path&0xFFFFF);
      fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644); //TODO maybe O_TRUNC not needed
      if(fd<0){
        log_.unlock();
        LOG("ERROR, failed to open BROADCAST LOG %s\n",filename);
        return;}}
    write(fd,p,len);
    write(fd,hash,32);
    write(fd,pkey,32);
    write(fd,&msid,sizeof(uint32_t));
    write(fd,&mpos,sizeof(uint32_t));
    log_.unlock();
  }

  bool process_message(message_ptr msg)
  { assert(msg->data!=NULL);
    if(msg->now<last_srvs_.now){
      LOG("ERROR MSG %04X:%08X too old :-( (%08X<%08X)\n",msg->svid,msg->msid,msg->now,last_srvs_.now);
      return(false);}
    LOG("PROCESS MSG %04X:%08X\n",msg->svid,msg->msid);
    char* p=(char*)msg->data+4+64+10;
    char filename[64];
    sprintf(filename,"usr/%04X.dat",msg->svid);
    int fd=open(filename,O_RDWR|O_CREAT,0644); //use memmory mapped file due to unordered transactions
    if(fd<0){
      LOG("ERROR, failed to open bank register %04X, fatal\n",msg->svid);
      exit(-1);}
    std::map<uint64_t,log_t> log;
    std::map<uint32_t,user_t> changes;
    std::map<uint32_t,user_t> undo;
    std::map<uint32_t,int64_t> local_deposit;
    std::map<uint64_t,int64_t> txs_deposit;
    std::map<uint64_t,std::vector<uint32_t>> txs_bnk; //create new bank
    std::map<uint64_t,std::vector<get_t>> txs_get; //set lock / withdraw
    std::map<uint64_t,std::vector<usr_t>> txs_usr; //remote account request
    std::map<uint64_t,std::vector<uok_t>> txs_uok; //remote account accept
    uint32_t users=srvs_.nodes[msg->svid].users;
    uint32_t ousers=users;
    uint32_t lpath=srvs_.now;
    uint32_t now=time(NULL); //needed for the log
    uint32_t lpos=1; // needed for log
    hash_t new_bky;
    bool set_bky=false;
    int mpt_size=0;
    std::vector<uint16_t> mpt_bank; // for MPT to local bank
    std::vector<uint32_t> mpt_user; // for MPT to local bank
    std::vector< int64_t> mpt_mass; // for MPT to local bank
    uint64_t csum[4]={0,0,0,0};
     int64_t weight=0; //FIXME fix weight calculation later !!! (include correct fee handling)
    uint64_t ppi=make_ppi(msg->msid,msg->svid,msg->svid);
    int64_t local_fee=0;
    int64_t lodiv_fee=0;
    int64_t myput_fee=0; // remote bank_fee for local bank
    while(p<(char*)msg->data+msg->len){
      uint32_t luser=0;
      uint16_t lnode=0;
      int64_t deduct=0;
      int64_t fee=0;
      int64_t remote_fee=0;
      uint32_t mpos=((uint8_t*)p-(uint8_t*)msg->data);
      /************* START PROCESSING **************/
      user_t* usera=NULL;
      usertxs utxs;
      if(!utxs.parse(p)){
        std::cerr<<"ERROR: failed to parse transaction\n";
        return(false);}
      utxs.print_head();
      if(*p==TXSTYPE_CON){
        //std::cerr<<"INFO: parsed CON transaction\n";
        srvs_.nodes[msg->svid].port=utxs.abank;
        srvs_.nodes[msg->svid].ipv4=utxs.auser;
        p+=utxs.size;
        continue;}
      if(*p>=TXSTYPE_INF){
        std::cerr<<"ERROR: unknown transaction\n";
        return(false);}
      if(utxs.ttime>lpath+BLOCKSEC+5){ // remember that values are unsigned !
        //std::cerr<<"ERROR: time in the future block\n";
	LOG("ERROR: time in the future block time:%08X block:%08X limit %08X\n",
	  utxs.ttime,lpath,lpath+BLOCKSEC+5);
        return(false);}
      //if(utxs.abank!=msg->svid && *p!=TXSTYPE_USR){
      if(utxs.abank!=msg->svid){
        std::cerr<<"ERROR: bad bank\n";
        utxs.print_head();
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
          LOG("ERROR: bad target user id %08X\n",luser);
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
          //dividend(*usera); // just a test, no update of account
          //if(usera->weight>TXS_DIV_FEE){ // require at least TXS_DIV_FEE otherwiese dust txs will keep user alive
          if(!(usera->stat&USER_STAT_DELETED)){
            LOG("ERROR, overwriting active account %04X:%08X [weight:%016lX]\n",
              utxs.bbank,luser,usera->weight);
            return(false);}
          local_fee+=delta;
          weight-=delta;}
        else{ //TODO, consider locking
          user_t u;
          bzero(&u,sizeof(user_t));
          users++;
          changes[luser]=u;
          usera=&changes[luser];}
	srvs_.xor4(csum,usera->csum);
        srvs_.init_user(*usera,msg->svid,luser,(*p==TXSTYPE_USR?USER_MIN_MASS:0),(uint8_t*)lpkey,utxs.ttime,utxs.abank,utxs.auser);
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
          txs_uok[ppi].push_back(uok);
          p+=utxs.size;
          continue;}}
      if(utxs.auser>=users){
        //std::cerr<<"ERROR, bad userid "<<utxs.abank<<":"<<utxs.auser<<"\n";
        LOG("ERROR, bad userid %04X:%08X\n",utxs.abank,utxs.auser);
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
      //if(!offi_.get_user(usera,utxs.abank,utxs.auser)){
      //  std::cerr<<"ERROR: read user failed\n";
      //  return(false);}
      if(utxs.wrong_sig((uint8_t*)p,(uint8_t*)usera->hash,(uint8_t*)usera->pkey)){
        std::cerr<<"ERROR: bad signature\n";
        return(false);}
      if(usera->msid!=utxs.amsid){
        //std::cerr<<"ERROR: bad msid ("<<usera->msid<<"<>"<<utxs.amsid<<")\n";
        LOG("ERROR: bad msid %04X:%08X\n",usera->msid,utxs.amsid);
        return(false);}
      //process transactions
      if(usera->time+2*LOCK_TIME<lpath && usera->user && usera->node && (usera->user!=utxs.auser || usera->node!=utxs.abank)){//check account lock
        if(*p!=TXSTYPE_PUT || utxs.abank!=utxs.bbank || utxs.auser!=utxs.buser || utxs.tmass!=0){
          std::cerr<<"ERROR: account locked, send 0 to yourself and wait for unlock\n";
          return(false);}}
      else if(*p==TXSTYPE_BRO){
        log_broadcast(lpath,p,utxs.size,usera->hash,usera->pkey,msg->msid,mpos);
        //utxs.print_broadcast(p);
        fee=TXS_BRO_FEE(utxs.bbank);}
      else if(*p==TXSTYPE_PUT){
        if(utxs.tmass<0){ //sending info about negative values is allowed to fascilitate exchanges
          utxs.tmass=0;}
        //if(utxs.abank!=utxs.bbank && utxs.auser!=utxs.buser && !check_user(utxs.bbank,utxs.buser))
        if(!srvs_.check_user(utxs.bbank,utxs.buser)){
          // does not check if account closed [consider adding this slow check]
          //std::cerr<<"ERROR: bad target user ("<<utxs.bbank<<":"<<utxs.buser<<")\n";
          LOG("ERROR: bad target user %04X:%08X\n",utxs.bbank,utxs.buser);
          return(false);}
        if(utxs.bbank==utxs.abank){
          local_deposit[utxs.buser]+=utxs.tmass;}
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
            std::cerr<<"ERROR: only positive non-zero transactions allowed in MPT\n";
            return(false);}
          if(out.find(to.big)!=out.end()){
            LOG("ERROR: duplicate target: %04X:%08X\n",tbank,tuser);
            return(false);}
          if(!srvs_.check_user((uint16_t)tbank,tuser)){
            LOG("ERROR: bad target user %04X:%08X\n",utxs.bbank,utxs.buser);
            return(false);}
          out.insert(to.big);
          if((uint16_t)tbank==utxs.abank){
            local_deposit[tuser]+=tmass;}
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
          txs_usr[ppi].push_back(usr);
          if(utxs.bbank==opts_.svid){ //respond to account creation request
            ofip_add_remote_user(utxs.abank,utxs.auser,usera->pkey);}}
        deduct=USER_MIN_MASS;
        if(utxs.abank!=utxs.bbank){
          fee=TXS_USR_FEE;}
        else{
          fee=TXS_MIN_FEE;}}
      else if(*p==TXSTYPE_BNK){ // we will get a confirmation from the network
        txs_bnk[ppi].push_back(utxs.auser);
        deduct=BANK_MIN_TMASS;
        fee=TXS_BNK_FEE;}
      else if(*p==TXSTYPE_GET){
        if(utxs.abank==utxs.bbank){
          //std::cerr<<"ERROR: bad bank ("<<utxs.bbank<<"), use PUT\n";
          LOG("ERROR: bad bank %04X, use PUT\n",utxs.bbank);
          return(false);}
        uint64_t ppb=make_ppi(msg->msid,msg->svid,utxs.bbank);
        get_t get;
        get.auser=utxs.auser;
        get.buser=utxs.buser;
        memcpy(get.pkey,usera->pkey,32);
        txs_get[ppb].push_back(get);
        fee=TXS_GET_FEE;}
      else if(*p==TXSTYPE_KEY){
        memcpy(usera->pkey,utxs.key(p),32);
        fee=TXS_KEY_FEE;}
      else if(*p==TXSTYPE_BKY){ // we will get a confirmation from the network
        if(utxs.auser){
          LOG("ERROR: bad user %04X for this bank changes\n",utxs.auser);
          return(false);}
        if(memcmp(srvs_.nodes[msg->svid].pk,utxs.opkey(p),32)){
          std::cerr<<"ERROR: bad current key\n";
          return(false);}
	set_bky=true;
        memcpy(new_bky,utxs.key(p),sizeof(hash_t));
        fee=TXS_BKY_FEE;}
      int64_t div=dividend(*usera,lodiv_fee); //do this before checking balance
      if(div!=(int64_t)0x8FFFFFFFFFFFFFFF){
LOG("DIV: pay to %04X:%08X (%016lX)\n",msg->svid,utxs.auser,div);
        weight+=div;}
      //if(deduct+fee+(utxs.auser?USER_MIN_MASS:BANK_MIN_UMASS)>usera->weight){
      if(deduct+fee+(utxs.auser?0:BANK_MIN_UMASS)>usera->weight){ //network accepts total withdrawal from user
        //std::cerr<<"ERROR: too low balance ("<<deduct<<"+"<<fee<<"+"<<USER_MIN_MASS<<">"<<usera->weight<<")\n";
        LOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
          deduct,fee,(uint64_t)(utxs.auser?0:BANK_MIN_UMASS),usera->weight);
        return(false);}
      if(msg->svid!=opts_.svid){
        if((*p==TXSTYPE_PUT || *p==TXSTYPE_GET) && utxs.bbank==opts_.svid){
          uint64_t key=(uint64_t)utxs.buser<<32;
          key|=lpos++;
          log_t blog;
          blog.time=now;
          blog.type=*p|0x8000; //incoming
          blog.node=utxs.abank;
          blog.user=utxs.auser;
          blog.umid=utxs.amsid;
          blog.nmid=msg->msid; //can be overwritten with info
          blog.mpos=mpos; //can be overwritten with info
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
          blog.mpos=mpos; //can be overwritten with info
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
      usera->weight+=local_deposit[utxs.auser]-deduct-fee;
      weight+=local_deposit[utxs.auser]-deduct-fee;
      local_deposit[utxs.auser]=0;//to find changes[utxs.auser]
      local_fee+=fee-remote_fee;
      p+=utxs.size;}
    //commit local changes
    user_t u;
    //int offset=(char*)&u.weight-(char*)&u;
    const int offset=(char*)&u+sizeof(user_t)-(char*)&u.rpath;
    //FIXME, lock here in case there is a bank read during sync, or accept errors when sending bank
    for(auto it=local_deposit.begin();it!=local_deposit.end();it++){
      auto jt=changes.find(it->first);
      if(jt!=changes.end()){
        srvs_.xor4(csum,jt->second.csum);
        jt->second.weight+=it->second;
	weight+=it->second;
	srvs_.user_csum(jt->second,msg->svid,it->first);
        srvs_.xor4(csum,jt->second.csum);
        lseek(fd,jt->first*sizeof(user_t),SEEK_SET);
        write(fd,&jt->second,sizeof(user_t));}//}
      else{
        lseek(fd,it->first*sizeof(user_t),SEEK_SET);
        read(fd,&u,sizeof(user_t));
        undo[it->first]=u;
        srvs_.xor4(csum,u.csum);
        int64_t div=dividend(u,lodiv_fee); // store local fee
        if(div!=(int64_t)0x8FFFFFFFFFFFFFFF){
LOG("DIV: pay to %04X:%08X (%016lX)\n",msg->svid,it->first,div);
          weight+=div;}
        u.weight+=it->second;
	weight+=it->second;
	srvs_.user_csum(u,msg->svid,it->first);
        srvs_.xor4(csum,u.csum);
        lseek(fd,-offset,SEEK_CUR);
        write(fd,&u.rpath,offset);}}
    local_deposit.clear();
    changes.clear();
    close(fd);
    //log bank fees
    int64_t profit=BANK_PROFIT(local_fee+lodiv_fee)-MESSAGE_FEE;
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
      memcpy(alog.info+2*sizeof(int64_t),&myput_fee,sizeof(int64_t));
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
      memcpy(alog.info,&local_fee,sizeof(int64_t));
      memcpy(alog.info+sizeof(int64_t),&lodiv_fee,sizeof(int64_t));
      memcpy(alog.info+2*sizeof(int64_t),&myput_fee,sizeof(int64_t));
      bzero(alog.info+3*sizeof(int64_t),sizeof(int64_t));
      log[0]=alog;}
    bank_fee[msg->svid]+=profit;
    msg->save_undo(undo,ousers,csum,weight,profit,srvs_.nodes[msg->svid].msha,srvs_.nodes[msg->svid].mtim);
    srvs_.nodes[msg->svid].weight+=weight;
    srvs_.xor4(srvs_.nodes[msg->svid].hash,csum);
    srvs_.save_undo(msg->svid,undo,ousers); //databank, will change srvs_.nodes[msg->svid].users
    //commit remote deposits
    deposit_.lock();
    for(auto it=txs_deposit.begin();it!=txs_deposit.end();it++){
      deposit[it->first]+=it->second;}
    //check for maximum deposits.size(), if too large, save old deposits and work on new ones;
    deposit_.unlock();
    txs_deposit.clear();
    //store block transactions
    blk_.lock();
    if(set_bky){
      memcpy(srvs_.nodes[msg->svid].pk,new_bky,32);
      if(msg->svid==opts_.svid){
        if(!srvs_.find_key(new_bky,skey)){
          LOG("ERROR, failed to change to new bank key, fatal!\n");
          exit(-1);}}}
    blk_bnk.insert(txs_bnk.begin(),txs_bnk.end());
    blk_get.insert(txs_get.begin(),txs_get.end());
    blk_.unlock();
    //if(do_sync){ //FIXME, do not duplicate previous log data !!!
    //  put_log(msg->svid,log);} //put_blklog
    //else{
    put_msglog(srvs_.now,msg->svid,msg->msid,log);
    //} //put_msglog
    return(true);
  }

  int open_bank(uint16_t svid) //
  { char filename[64];
    LOG("OPEN usr/%04X.dat",svid);
    sprintf(filename,"usr/%04X.dat",svid);
    int fd=open(filename,O_RDWR|O_CREAT,0644);
    if(fd<0){
      LOG("ERROR, failed to open bank register %04X, fatal\n",svid);
      exit(-1);}
    return(fd);
  }

  void commit_block(std::set<uint16_t>& update) //assume single thread
  { mydiv_fee=0;
    myusr_fee=0;
    myget_fee=0;
    uint32_t lpos=1;
    std::map<uint64_t,log_t> log;

    //match remote account transactions
    std::map<uin_t,uint32_t,uin_cmp> uin; //waiting remote account requests
    for(auto it=blk_usr.begin();it!=blk_usr.end();it++){
      uint16_t abank=ppi_abank(it->first);
      for(auto tx=it->second.begin();tx!=it->second.end();tx++){
        uin_t nuin={tx->bbank,abank,tx->auser,
          tx->pkey[ 0],tx->pkey[ 1],tx->pkey[ 2],tx->pkey[ 3],tx->pkey[ 4],tx->pkey[ 5],tx->pkey[ 6],tx->pkey[ 7],
          tx->pkey[ 8],tx->pkey[ 9],tx->pkey[10],tx->pkey[11],tx->pkey[12],tx->pkey[13],tx->pkey[14],tx->pkey[15],
          tx->pkey[16],tx->pkey[17],tx->pkey[18],tx->pkey[19],tx->pkey[20],tx->pkey[21],tx->pkey[22],tx->pkey[23],
          tx->pkey[24],tx->pkey[25],tx->pkey[26],tx->pkey[27],tx->pkey[28],tx->pkey[29],tx->pkey[30],tx->pkey[31]};
        uin[nuin]++;}}
    for(auto it=blk_uok.begin();it!=blk_uok.end();it++){ //send funds from matched transactions to new account
      uint16_t abank=ppi_abank(it->first);
      for(auto tx=it->second.begin();tx!=it->second.end();tx++){
        uin_t nuin={abank,tx->bbank,tx->buser,
          tx->pkey[ 0],tx->pkey[ 1],tx->pkey[ 2],tx->pkey[ 3],tx->pkey[ 4],tx->pkey[ 5],tx->pkey[ 6],tx->pkey[ 7],
          tx->pkey[ 8],tx->pkey[ 9],tx->pkey[10],tx->pkey[11],tx->pkey[12],tx->pkey[13],tx->pkey[14],tx->pkey[15],
          tx->pkey[16],tx->pkey[17],tx->pkey[18],tx->pkey[19],tx->pkey[20],tx->pkey[21],tx->pkey[22],tx->pkey[23],
          tx->pkey[24],tx->pkey[25],tx->pkey[26],tx->pkey[27],tx->pkey[28],tx->pkey[29],tx->pkey[30],tx->pkey[31]};
        if(uin.find(nuin)!=uin.end() && uin[nuin]>0){
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
            //put_log(tx->bbank,tx->buser,alog); //put_blklog
          uin[nuin]--;}
        else if(tx->bbank==opts_.svid){ // no matching _USR transaction found
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
          log[key]=alog;}}}
          //put_log(tx->bbank,tx->buser,alog); //put_blklog
    for(auto it=uin.begin();it!=uin.end();it++){ //send back funds from unmatched transactions
      uint32_t n=it->second;
      for(;n>0;n--){
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
          log[key]=alog;}}}
          //put_log(it->first.abank,it->first.auser,alog); //put_blklog

    //create new banks
    if(!blk_bnk.empty()){
      std::set<uint64_t> new_bnk; // list of available banks for takeover
      uint16_t peer=0;
      for(auto it=srvs_.nodes.begin()+1;it!=srvs_.nodes.end();it++,peer++){ // start with bank=1
        if(it->mtim+BANK_MIN_MTIME<srvs_.now && it->weight<BANK_MIN_TMASS){
          uint64_t bnk=it->weight<<16;
          bnk|=peer;
          new_bnk.insert(bnk);}}
      for(auto it=blk_bnk.begin();it!=blk_bnk.end();it++){
        uint16_t abank=ppi_abank(it->first);
        int fd=open_bank(abank);
        //update.insert(abank); no update here
        for(auto tx=it->second.begin();tx!=it->second.end();tx++){
          user_t u;
          lseek(fd,(*tx)*sizeof(user_t),SEEK_SET);
          read(fd,&u,sizeof(user_t));
          uint16_t peer=0;
          // create new bank and new user
          if(!new_bnk.empty()){
            auto bi=new_bnk.begin();
            peer=(*bi)&0xffff;
            new_bnk.erase(bi);
            LOG("BANK, overwrite %04X\n",peer);
            srvs_.put_node(u,peer,abank,*tx);} //save_undo() in put_node() !!!
          else if(srvs_.nodes.size()<BANK_MAX-1){
            peer=srvs_.add_node(u,abank,*tx); //deposits BANK_MIN_TMASS
            bank_fee.resize(srvs_.nodes.size());
            LOG("BANK, add new bank %04X\n",peer);}
          else{
            close(fd);
            LOG("BANK, can not create more banks\n");
            //goto BLK_END;
            union {uint64_t big;uint32_t small[2];} to;
            to.small[0]=*tx;
            to.small[1]=abank;
            deposit[to.big]+=BANK_MIN_TMASS;
            if(*tx){
              bank_fee[to.small[1]]-=BANK_PROFIT(TXS_LNG_FEE(BANK_MIN_TMASS));} //else would generate extra fee
            if(abank==opts_.svid){
              uint64_t key=(uint64_t)*tx<<32;
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
              //put_log(abank,*tx,alog); //put_blklog
            continue;}
          update.insert(peer);
          if(abank==opts_.svid){
            uint64_t key=(uint64_t)*tx<<32;
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
            log[key]=alog;}}
            //put_log(abank,*tx,alog); //put_blklog
        close(fd);}
      //BLK_END:
      blk_bnk.clear();}

    //withdraw funds
    uint16_t svid=0;
    int fd=0;
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
        fd=open_bank(svid);}
      union {uint64_t big;uint32_t small[2];} to;
      to.small[1]=abank; //assume big endian
      for(auto tx=it->second.begin();tx!=it->second.end();tx++){
        user_t u;
        lseek(fd,tx->buser*sizeof(user_t),SEEK_SET);
        read(fd,&u,sizeof(user_t));
        if(!memcmp(u.pkey,tx->pkey,32)){ //FIXME, add transaction fees processing
          if(u.time+2*LOCK_TIME>srvs_.now){
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
LOG("DIV: pay to %04X:%08X (%016lX)\n",bbank,tx->buser,div);
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
          write(fd,&u,sizeof(user_t));
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
            ofip_gup_push(g);}}}}
    if(svid){
      srvs_.save_undo(svid,undo,0);
      undo.clear();}
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
    char filename[64];
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
      LOG("DIVIDEND to usr/%04X.dat\n",svid);
      sprintf(filename,"usr/%04X.dat",svid);
      int fd=open(filename,O_RDWR,0644);
      if(fd<0){
        LOG("ERROR, failed to open bank register %04X, fatal\n",svid);
        exit(-1);}
      for(;;user+=segment*(period-1)){
        lseek(fd,user*sizeof(user_t),SEEK_SET);
        for(int i=0;i<segment;i++,user++){
          if(user>=uend){
            goto NEXTBANK;}
          read(fd,&u,sizeof(user_t));
          if(!u.msid){
            LOG("ERROR, failed to read user %04X:%08X, fatal\n",svid,user);
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
LOG("DIV: to %04X:%08X (%016lX)\n",svid,user,div);
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
            write(fd,&u.rpath,offset);}}}
      NEXTBANK:
      update.insert(svid);
      close(fd);
      srvs_.save_undo(svid,undo,0);
      undo.clear();}
  }

  void commit_deposit(std::set<uint16_t>& update) //assume single thread, TODO change later !!!
  { //uint32_t now=time(NULL); //for the log
    //std::map<uint64_t,log_t> log;
    char filename[64];
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
        LOG("DEPOSIT to usr/%04X.dat\n",svid);
        sprintf(filename,"usr/%04X.dat",svid);
        fd=open(filename,O_RDWR,0644);
        if(fd<0){
          LOG("ERROR, failed to open bank register %04X, fatal\n",svid);
          exit(-1);}}
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
LOG("DIV: during deposit to %04X:%08X (%016lX) (%016lX)\n",svid,user,div,it->second);
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
      write(fd,&u.rpath,offset);}
    if(lastsvid){
      close(fd);
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
      char filename[64];
      sprintf(filename,"usr/%04X.dat",svid);
      int fd=open(filename,O_RDWR,0644);
      if(fd<0){
        LOG("ERROR, failed to open bank register %04X, fatal\n",svid);
        exit(-1);}
      read(fd,&u,sizeof(user_t));
      std::map<uint32_t,user_t> undo;
      undo.emplace(0,u);
      int64_t fee=0;
      int64_t div=dividend(u,fee);
      bank_fee[svid]+=BANK_PROFIT(fee);
      if(div==(int64_t)0x8FFFFFFFFFFFFFFF){
        div=0;}
      else{
        mydiv_fee+=BANK_PROFIT(fee);
LOG("DIV: during bank_fee to %04X (%016lX)\n",svid,div);
        }
      int64_t buser_fee=BANK_USER_FEE(srvs_.nodes[svid].users);
      int64_t profit=bank_fee[svid]-buser_fee;
      LOG("BANK_PROFIT %016lX to usr/%04X.dat (%ld)\n",profit,svid,profit);
      //int64_t before=u.weight; 
      if(profit<-u.weight){
       profit=-u.weight;}
      u.weight+=profit;
      //int64_t after=u.weight; 
      srvs_.nodes[svid].weight+=profit+div;
      u.rpath=srvs_.now;
      if(svid==opts_.svid && !do_sync && ofip!=NULL){
        ofip_add_remote_deposit(0,profit);} //DEPOSIT
      srvs_.xor4(srvs_.nodes[svid].hash,u.csum);
      srvs_.user_csum(u,svid,0);
      srvs_.xor4(srvs_.nodes[svid].hash,u.csum);
      lseek(fd,-offset,SEEK_CUR);
      write(fd,&u.rpath,offset);
      close(fd);
      srvs_.save_undo(svid,undo,0);
      if(svid==opts_.svid){
        uint64_t key=(uint64_t)svid<<32;
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
        log[key]=alog;}}
        //put_log(svid,0,alog);  //put_blklog
    bank_fee.clear();
    bank_fee.resize(srvs_.nodes.size());
    put_msglog(srvs_.now,0,0,log);
  }

  //bool accept_message(uint32_t lastmsid)
  bool accept_message()
  { //FIXME, add check for vulnerable time
    //return(lastmsid<=msid_ && msid_==srvs_.nodes[opts_.svid].msid); //quick fix, in future lastmsid==msid_
    return(msid_==srvs_.nodes[opts_.svid].msid); //quick fix, in future lastmsid==msid_
  }

  void update_list(std::vector<uint64_t>& txs,std::vector<uint64_t>& dbl,uint16_t peer_svid)
  { txs_.lock();
    for(auto me=txs_msgs_.begin();me!=txs_msgs_.end();me++){ // adding more messages, TODO this should not be needed ... all messages should have path=srvs_.now
      if(me->second->status==MSGSTAT_VAL){
        union {uint64_t num; uint8_t dat[8];} h;
        h.num=me->first;
        h.dat[0]=MSGTYPE_PUT;
        h.dat[1]=me->second->hashval(peer_svid); //data[4+(peer_svid%64)]
        me->second->sent_erase(peer_svid);
        txs.push_back(h.num);}}
    txs_.unlock();
    dbl_.lock();
    for(auto me=dbl_msgs_.begin();me!=dbl_msgs_.end();me++){ // adding more messages, TODO this should not be needed ... all messages should have path=srvs_.now
      if(me->second->status==MSGSTAT_VAL){
        union {uint64_t num; uint8_t dat[8];} h;
        h.num=me->first;
        h.dat[0]=MSGTYPE_DBP;
        h.dat[1]=0;//me->second->data[4+(peer_svid%64)];
        me->second->sent_erase(peer_svid);
        dbl.push_back(h.num);}}
    dbl_.unlock();
  }

  void finish_block()
  { 
    txs_.lock();
    std::map<uint16_t,message_ptr> last_block_svid_msgs=last_svid_msgs; // last block latest validated message from server, should change this to now_svid_msgs
    if(!do_sync){ // not during make_chain()
      for(auto it=winner->svid_have.begin();it!=winner->svid_have.end();it++){
        //last_block_svid_msgs.erase(*it);
        uint16_t svid=it->first;
        uint32_t msid=it->second.msid;
        if(!msid){
          LOG("WARNING msid==0 for svid:%04X (svid_have)\n",(uint32_t)svid); //FIXME, this caused errors
          //try a fix:
          last_block_svid_msgs.erase(it->first);
          continue;}
        uint8_t* sigh=it->second.sigh;
        union {uint64_t num; uint8_t dat[8];} h;
        h.dat[0]=0; // hash
        h.dat[1]=0; // message type
        memcpy(h.dat+2,&msid,4);
        memcpy(h.dat+6,&svid,2);
//FIXME, include dblspend message search
        auto mi=txs_msgs_.lower_bound(h.num);
        while(1){
          if((mi==txs_msgs_.end()) || ((mi->first & 0xFFFFFFFFFFFF0000L)!=(h.num))){
            LOG("FATAL, could not find svid_have message :-( %016lX\n",h.num);
            exit(-1);}
          if(!memcmp(sigh,mi->second->sigh,sizeof(hash_t))){
            last_block_svid_msgs[it->first]=mi->second;
            break;}
          mi++;}}
      for(auto it=winner->svid_miss.begin();it!=winner->svid_miss.end();it++){
        uint16_t svid=it->first;
        uint32_t msid=it->second.msid;
        if(!msid){
          LOG("WARNING msid==0 for svid:%04X (svid_miss)\n",(uint32_t)svid); //FIXME, this caused errors
          //try a fix:
          last_block_svid_msgs.erase(it->first);
          continue;}
        uint8_t* sigh=it->second.sigh;
        union {uint64_t num; uint8_t dat[8];} h;
        h.dat[0]=0; // hash
        h.dat[1]=0; // message type
        memcpy(h.dat+2,&msid,4);
        memcpy(h.dat+6,&svid,2);
//FIXME, include dblspend message search
        auto mi=txs_msgs_.lower_bound(h.num);
        while(1){
          if((mi==txs_msgs_.end()) || ((mi->first & 0xFFFFFFFFFFFF0000L)!=(h.num))){
            LOG("FATAL, could not find svid_miss message :-( %016lX\n",h.num);
            exit(-1);}
          if(!memcmp(sigh,mi->second->sigh,sizeof(hash_t))){
            last_block_svid_msgs[it->first]=mi->second;
            break;}
          mi++;}}}
    txs_.unlock();

    // save list of final message hashes
    char filename[64];
    sprintf(filename,"blk/%03X/%05X/delta.txt",srvs_.now>>20,srvs_.now&0xFFFFF); // size depends on the time_ shift and maximum number of banks (0xffff expected) !!
    FILE *fp=fopen(filename,"w");
    char* hash=(char*)malloc(2*sizeof(hash_t));
    for(auto it=last_block_svid_msgs.begin();it!=last_block_svid_msgs.end();it++){
      //node* nod=&srvs_.nodes[it->first];
      //FIXME, this should not be needed !!! this will be obtained by undoing messages
      //nod->msid=it->second->msid; //TODO, it should be in the nodes already
      //nod->mtim=it->second->now; //TODO, it should be in the nodes already
      //memcpy(nod->msha,it->second->sigh,sizeof(hash_t)); //TODO, it should be in the nodes already
      ////setting nod->hash is missing here
      ed25519_key2text(hash,it->second->sigh,sizeof(hash_t));
      LOG("DELTA: %d %d %.*s\n",it->first,it->second->msid,(int)(2*sizeof(hash_t)),hash);
      fprintf(fp,"%d %d %.*s\n",it->first,it->second->msid,(int)(2*sizeof(hash_t)),hash);}
    // confirm hash;
    hash_s last_block_message;
    message_shash(last_block_message.hash,last_block_svid_msgs);
    if(!do_sync){
      cand_.lock();
      auto ca=candidates_.find(last_block_message);
      if(ca==candidates_.end() || memcmp(last_block_message.hash,ca->first.hash,sizeof(hash_t))){
        std::cerr << "FATAL, failed to confirm block hash\n";
        cand_.unlock();
        exit(-1);}
      cand_.unlock();}
    ed25519_key2text(hash,last_block_message.hash,sizeof(hash_t));
    LOG("DELTA: 0 0 %.*s\n",(int)(2*sizeof(hash_t)),hash);
    fprintf(fp,"0 0 %.*s\n",(int)(2*sizeof(hash_t)),hash);
    fclose(fp); //TODO consider updating peers about correct block msgs hash

    std::stack<message_ptr> remove_msgs;
    std::stack<message_ptr> invalidate_msgs;
    message_queue commit_msgs; //std::forward_list<message_ptr> commit_msgs;
    std::map<uint64_t,message_ptr> last_block_all_msgs; // last block all validated message from server, should change this to now_svid_msgs
    sprintf(filename,"blk/%03X/%05X/block.txt",srvs_.now>>20,srvs_.now&0xFFFFF); // size depends on the time_ shift and maximum number of banks (0xffff expected) !!
    fp=fopen(filename,"w");
    uint32_t txcount=0;
    uint16_t nsvid=0; // current svid processed
    uint32_t minmsid=0; // first msid to be processed
    uint32_t maxmsid=0; // last msid to be proceesed
    bool remove=false;
    txs_.lock();
    for(auto mj=txs_msgs_.begin();mj!=txs_msgs_.end();){ // adding more messages, TODO this should not be needed ... all messages should have path=srvs_.now
      auto mi=mj++;
      if(mi->second->path>0 && mi->second->path<srvs_.now){
        LOG("ERASE message %04X:%08X [len:%d]\n",mi->second->svid,mi->second->msid,mi->second->len);
        mi->second->remove_undo();
        txs_msgs_.erase(mi); // forget old messages
        continue;}
      if(mi->second->path==0 && mi->second->len==message::header_length &&
         mi->second->msid<last_srvs_.nodes[mi->second->svid].msid){
        LOG("CLEAN JUNK message %04X:%08X [len:%d]\n",mi->second->svid,mi->second->msid,mi->second->len);
        txs_msgs_.erase(mi); // forget old messages
        continue;}
      if(nsvid!=mi->second->svid){
        remove=0;
        nsvid=mi->second->svid;
        if(last_srvs_.nodes.size()<nsvid){
          minmsid=1;}
	else{
          minmsid=last_srvs_.nodes[nsvid].msid+1;}
	auto lbsm=last_block_svid_msgs.find(nsvid);
        if(lbsm==last_block_svid_msgs.end()){
          maxmsid=0;}
        else{
          maxmsid=lbsm->second->msid;}
        LOG("RANGE %d %d %d\n",nsvid,minmsid,maxmsid);
        fprintf(fp,"RANGE %d %d %d\n",nsvid,minmsid,maxmsid);}
      if(maxmsid==0xffffffff && mi->second->msid<0xffffffff){ // remove messages from dbl_spend server
        LOG("REMOVE message %04X:%08X later! (DBL-spend server)\n",nsvid,mi->second->msid);
        remove_msgs.push(mi->second);
        continue;}
      if(mi->second->msid>maxmsid){
        if(mi->second->now<last_srvs_.now){ // remove messages if failed to be included in 2 blocks
          //if(!remove){ //this couases errors, because of timing :-(
          //  svid_msid_rollback(mi->second);}
          remove=true;}
        if(remove){
          if(mi->second->len>message::header_length){
            LOG("\n\nREMOVE message %04X:%08X (msg.now:%08X<last.now:%08X) [len:%d]:-(\n\n\n",
              nsvid,mi->second->msid,mi->second->now,last_srvs_.now,mi->second->len);
            if(mi->second->svid==opts_.svid){
//FIXME, sign message with new time
exit(-1);


            }
            remove_msgs.push(mi->second);}
          else{
            ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
            LOG("IGNOR: %04X:%08X %.16s %016lX (%016lX) ... INIT JUNK ...\n",
              nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first);}}
        else{
          if(mi->second->status&MSGSTAT_VAL){
            LOG("INVALIDATE message %04X:%08X later!\n",nsvid,mi->second->msid);
            invalidate_msgs.push(mi->second);}
          LOG("MOVING message %04X:%08X to %08X/\n",nsvid,mi->second->msid,srvs_.now+BLOCKSEC);
          mi->second->move(srvs_.now+BLOCKSEC);}}
      else{
        if(mi->second->len==message::header_length){
          ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
          LOG("IGNOR: %04X:%08X %.16s %016lX (%016lX)\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first);
          continue;}
        //assert(mi->second->path==srvs_.now); // check, maybe path is not assigned yet
	if(mi->second->path!=srvs_.now){
          ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
          LOG("PATH?: %04X:%08X %.16s %016lX (%016lX) path=%08X srvs_.now=%08X\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first,mi->second->path,srvs_.now);}
        if(maxmsid!=0xffffffff && mi->second->msid!=minmsid){
          ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
          LOG("?????: %04X:%08X %.16s %016lX (%016lX)\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first);
          LOG("ERROR, failed to read correct transaction order in block %04x:%08X<>%08X (max:%08X)\n",
            nsvid,mi->second->msid,minmsid,maxmsid);
          for(auto mx=txs_msgs_.begin();mx!=txs_msgs_.end();mx++){
            LOG("%016lX\n",mx->first);}
          exit(-1);}
        if(!(mi->second->status & MSGSTAT_VAL)){
          std::cerr << "ERROR, invalid transaction in block\n";
          exit(-1);}
        txcount++;
        minmsid++;
        commit_msgs.push_back(mi->second);
        last_block_all_msgs[txcount]=mi->second;
        ed25519_key2text(hash,mi->second->sigh,sizeof(hash_t));
        LOG("BLOCK: %d %d %.16s %016lX (%016lX)\n",nsvid,mi->second->msid,hash,mi->second->hash.num,mi->first);
        fprintf(fp,"%d %d %.*s\n",nsvid,mi->second->msid,(int)(2*sizeof(hash_t)),hash);}}
    txs_.unlock();
    srvs_.msg=last_block_all_msgs.size();
    srvs_.msg_put(last_block_all_msgs); //FIXME, add dbl_ messages !!! FIXME FIXME !!!
    ed25519_key2text(hash,srvs_.msghash,sizeof(hash_t));
    fprintf(fp,"0 0 %.*s\n",(int)(2*sizeof(hash_t)),hash);
    fclose(fp);
    for(;!remove_msgs.empty();remove_msgs.pop()){
      auto mi=remove_msgs.top();
      if(mi->status & MSGSTAT_VAL){ // invalidate first
	undo_message(mi);
        mi->status &= ~MSGSTAT_VAL;}
//FIXME, make sure this message is not in other queue !!!
//FIXME, remove from all places or mark as removed :-/
      LOG("REMOVING message %04X:%08X\n",mi->svid,mi->msid);
//FIXME, update peers.svid_msid_new !!!
      remove_message(mi);
      mi->remove();}
    check_.lock();
    for(;!invalidate_msgs.empty();invalidate_msgs.pop()){
      auto mi=invalidate_msgs.top();
      ed25519_key2text(hash,mi->sigh,sizeof(hash_t));
      LOG("INVALIDATING %04X:%08X %.16s %016lX %08X/\n",mi->svid,mi->msid,hash,mi->hash.num,mi->path);
      undo_message(mi);
      mi->status &= ~MSGSTAT_VAL;
      check_msgs_.push_front(mi);}
    check_.unlock();
    std::set<uint16_t> update; //TODO, remove this, quite useless
    for(auto mi=commit_msgs.begin();mi!=commit_msgs.end();mi++){
      update.insert((*mi)->svid);
      LOG("COMMITING message %04X:%08X [%08X]\n",(*mi)->svid,(*mi)->msid,(*mi)->now);}
    commit_block(update); // process bkn and get transactions
    commit_dividends(update);
    commit_deposit(update);
    commit_bankfee();
    std::cerr << "CHECK accounts\n"; //FIXME, remove later !!!
    for(auto it=update.begin();it!=update.end();it++){
      assert(*it<srvs_.nodes.size());
      if(!srvs_.check_nodehash(*it)){
        LOG("FATAL ERROR, failed to check the hash of bank %04X\n",*it);
        exit(-1);}}
    srvs_.finish(); //FIXME, add locking
    last_srvs_=srvs_; // consider not making copies of nodes
    memcpy(srvs_.oldhash,last_srvs_.nowhash,SHA256_DIGEST_LENGTH);
    period_start=srvs_.nextblock();
    vip_max=srvs_.update_vip();
    if(!do_sync){
      ofip_update_block(period_start,srvs_.now,commit_msgs,srvs_.div);
      free(hash);
      for(auto mj=cnd_msgs_.begin();mj!=cnd_msgs_.end();){
        auto mi=mj++;
        if(mi->second->msid<last_srvs_.now){
          cnd_msgs_.erase(mi);
          continue;}}
      for(auto mj=blk_msgs_.begin();mj!=blk_msgs_.end();){
        auto mi=mj++;
        if(mi->second->msid<last_srvs_.now){
          blk_msgs_.erase(mi);
          continue;}}}
    std::cerr << "NEW BLOCK created\n";
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
      bzero(hs.msha,SHA256_DIGEST_LENGTH);
      hs.msid=0;}
    hs.do_sync=do_sync;
    //ed25519_printkey(skey,32);
    //ed25519_printkey(pkey,32);
    hash_t empty;
    message_ptr msg(new message(MSGTYPE_INI,(uint8_t*)&hs,(int)sizeof(handshake_t),opts_.svid,msid_,skey,pkey,empty));
    return(msg);
  }

  uint32_t write_message(std::string line) // assume single threaded
  { static boost::mutex mtx_;
    mtx_.lock(); //FIXME, probably no lock needed
    int msid=++msid_; // can be atomic
    mtx_.unlock();
    // add location info. FIXME, set location to 0 before exit
    usertxs txs(TXSTYPE_CON,opts_.port&0xFFFF,opts_.ipv4,0);
    line.append((char*)txs.data,txs.size);
    message_ptr msg(new message(MSGTYPE_MSG,(uint8_t*)line.c_str(),(int)line.length(),opts_.svid,msid,skey,pkey,srvs_.nodes[opts_.svid].msha));
    //if(!msg->save()){
    //  LOG("ERROR, failed to save own message %08X, fatal\n",msid);
    //  exit(-1);}
    if(!txs_insert(msg)){
      std::cerr << "FATAL message insert error for own message, dying !!!\n";
      exit(-1);}
    writemsid();
    return(msid_);
    //update(msg);
  }

  void write_candidate(const hash_s& last_message)
  { do_vote=0;
    hash_t empty;
    message_ptr msg(new message(MSGTYPE_CND,last_message.hash,sizeof(hash_t),opts_.svid,srvs_.now,skey,pkey,empty)); //FIXME, consider msid=0 ???
//FIXME, is hash ok ?
    if(!cnd_insert(msg)){
      std::cerr << "FATAL message insert error for own message, dying !!!\n";
      exit(-1);}
    std::cerr << "SENDING candidate\n";
    update(msg); // update peers even if we are not an elector
  }

  candidate_ptr save_candidate(const hash_s& h,std::map<uint16_t,msidhash_t>& miss,std::map<uint16_t,msidhash_t>& have,uint16_t svid)
  { cand_.lock(); // lock only candidates
    auto it=candidates_.find(h);
    if(it==candidates_.end()){
      std::map<uint16_t,msidhash_t> new_svid_miss(miss);
      std::map<uint16_t,msidhash_t> new_svid_have(have);
      bool failed=false;
      for(auto jt=new_svid_have.begin();jt!=new_svid_have.end();){
        auto it=jt++;
        if(!it->second.msid){
          continue;}
        if(it->second.msid==0xffffffff){
          auto mt=new_svid_miss.find(it->first);
          if(mt!=new_svid_miss.end() && mt->second.msid!=0xffffffff){
            failed=true;}
          continue;}
        auto sm=last_svid_msgs.find(it->first);
        if(sm!=last_svid_msgs.end() && sm->second->msid==0xffffffff){ // we will not accespt double spend
          failed=true;}
        message_ptr pm=message_svidmsid(it->first,it->second.msid);
        if(pm==NULL){
          LOG("%04X CAND failed to find %04X:%08X from have list\n",svid,it->first,it->second.msid);
          new_svid_miss[it->first]=it->second;
          new_svid_have.erase(it);
          continue;}
        if(pm->got<srvs_.now+BLOCKSEC-MESSAGE_MAXAGE){ // we will not accept missing this message
          // do not accept candidates with missing: double spend, my messeges, old messages
          LOG("%04X ERROR old message %04X:%08X missing\n",svid,it->first,it->second.msid);
          failed=true;}}
      for(auto jt=new_svid_miss.begin();jt!=new_svid_miss.end();){
        auto it=jt++;
        if(!it->second.msid){
          continue;}
        if(it->second.msid==0xffffffff){
          auto mt=new_svid_have.find(it->first);
          if(mt!=new_svid_have.end() && mt->second.msid!=0xffffffff){
            failed=true;}
          continue;}
        auto sm=last_svid_msgs.find(it->first);
        if(sm!=last_svid_msgs.end() && sm->second->msid==0xffffffff){ // we will not accespt double spend
          failed=true;}
        message_ptr pm=message_svidmsid(it->first,it->second.msid);
        if(pm!=NULL){
          LOG("%04X CAND failed to find %04X:%08X from have list\n",svid,it->first,it->second.msid);
          new_svid_have[it->first]=it->second;
          new_svid_miss.erase(it);
          continue;}}
      candidate_ptr c_ptr(new candidate(new_svid_miss,new_svid_have,svid,failed));
      candidates_[h]=c_ptr;
      cand_.unlock();
      return(c_ptr);}
    else if(svid){
      it->second->peers.insert(svid);}
    cand_.unlock();
    return(it->second);
  }

  //void save_candidate(const hash_s& h,candidate_ptr c)
  //{
  //  cand_.lock(); // lock only candidates
  //  auto it=candidates_.find(h);
  //  if(it==candidates_.end()){
  //    candidates_[h]=c;}
  //  else if(c->peer){
  //    it->second->peers.insert(c->peer);}
  //  cand_.unlock();
  //}

  candidate_ptr known_candidate(const hash_s& h,uint16_t peer)
  { 
    cand_.lock(); // lock only candidates
    auto it=candidates_.find(h);
    if(it==candidates_.end()){
      cand_.unlock();
      return(NULL);}
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
      std::cerr << "FATAL message insert error for own message, dying !!!\n";
      exit(-1);}
    std::cerr << "SENDING block (update)\n";
    //deliver(msg);
    update(msg); //send, even if I am not VIP
// save signature in signature lists
//FIXME, save only if I am important
  }

  void peers() // connect new peers
  { 
    //FIXME, run this
    //return;
    while(1){
      boost::this_thread::sleep(boost::posix_time::seconds(5)); //will be interrupted to return
      uint32_t now=time(NULL)+5; // do not connect if close to block creation time
      now-=now%BLOCKSEC;
#if BLOCKSEC == 0x20
      if(peers_.size()>=2 || peers_.size()>(srvs_.nodes.size()-2)/2 || srvs_.now<now){
        continue;}
#else
      if(peers_.size()>=MIN_PEERS || peers_.size()>(srvs_.nodes.size()-2)/2 || srvs_.now<now){
        continue;}
#endif
      int16_t svid=(((uint64_t)random())%srvs_.nodes.size())&0xFFFF;
      if(!svid || svid==opts_.svid || !srvs_.nodes[svid].ipv4 || !srvs_.nodes[svid].port){
        //LOG("IGNORE CONNECT to %04X (%08X:%08X)\n",svid,srvs_.nodes[svid].ipv4,srvs_.nodes[svid].port);
        continue;}
      if(connected(svid)){
        //LOG("ALREADY CONNECT to %04X (%08X:%08X)\n",svid,srvs_.nodes[svid].ipv4,srvs_.nodes[svid].port);
        continue;}
      //LOG("TRY CONNECT to %04X (%08X:%08X)\n",svid,srvs_.nodes[svid].ipv4,srvs_.nodes[svid].port);
      connect(svid);}
  }

  void clock()
  { 
    //while(ofip==NULL){
    //  boost::this_thread::sleep(boost::posix_time::seconds(1));}
    //start office

    //TODO, number of validators should depend on opts_.
    if(!do_validate){
      do_validate=1;
      threadpool.create_thread(boost::bind(&server::validator, this));
      threadpool.create_thread(boost::bind(&server::validator, this));}

    //TODO, consider validating local messages faster to limit delay in this region
    //finish recycle submitted office messages
    while(msid_>srvs_.nodes[opts_.svid].msid){
      LOG("DEBUG, waiting to process local messages (%08X>%08X)\n",msid_,srvs_.nodes[opts_.svid].msid);
      boost::this_thread::sleep(boost::posix_time::seconds(1));}
    //recycle office message queue
    std::string line;
    if(ofip_get_msg(msid_+1,line)){
      LOG("DEBUG, adding office message queue (%08X)\n",msid_+1);
      write_message(line);
      while(msid_>srvs_.nodes[opts_.svid].msid){
        LOG("DEBUG, waiting to process local messages (%08X>%08X)\n",msid_,srvs_.nodes[opts_.svid].msid);
        boost::this_thread::sleep(boost::posix_time::seconds(1));}
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
        LOG("CLOCK: %02lX (check:%d wait:%d peers:%d hash:%8X now:%8X) [%s] (miss:%d:%016lX)\n",
          ((long)(srvs_.now+BLOCKSEC)-(long)now),(int)check_msgs_.size(),
          (int)wait_msgs_.size(),(int)peers_.size(),(uint32_t)*((uint32_t*)srvs_.nowhash),srvs_.now,plist,
          (int)missing_msgs_.size(),missing_msgs_.begin()->first);}
      else{
        LOG("CLOCK: %02lX (check:%d wait:%d peers:%d hash:%8X now:%8X) [%s]\n",
          ((long)(srvs_.now+BLOCKSEC)-(long)now),(int)check_msgs_.size(),
          (int)wait_msgs_.size(),(int)peers_.size(),(uint32_t)*((uint32_t*)srvs_.nowhash),srvs_.now,plist);}
      if(now>=(srvs_.now+BLOCKSEC) && do_block==0){
        std::cerr << "STOPing validation to start block\n";
        do_validate=0;
        threadpool.join_all();
        std::cerr << "STOPed validation to start block\n";
        //create message hash
        svid_.lock();
        last_svid_msgs.swap(svid_msgs_);
        svid_msgs_.clear();
        svid_.unlock();
        cand_.lock();
        electors.clear();
        candidates_.clear();
        cand_.unlock();
        message_shash(cand.hash,last_svid_msgs);
        message_msha(last_svid_msgs);
          message_ptr put_msg(new message(1+SHA256_DIGEST_LENGTH));
          put_msg->data[0]=MSGTYPE_STP;
          memcpy(put_msg->data+1,cand.hash,SHA256_DIGEST_LENGTH);
          char hash[2*SHA256_DIGEST_LENGTH]; hash[2*SHA256_DIGEST_LENGTH-1]='?';
          ed25519_key2text(hash,put_msg->data+1,SHA256_DIGEST_LENGTH);
          LOG("LAST HASH put %.*s\n",(int)(2*SHA256_DIGEST_LENGTH),hash);
          deliver(put_msg); // sets BLOCK_MODE for peers
        std::map<uint16_t,msidhash_t> miss;
        std::map<uint16_t,msidhash_t> have;
        save_candidate(cand,miss,have,opts_.svid);
        prepare_poll(); // sets do_vote
        do_block=1;
        do_validate=1;
        threadpool.create_thread(boost::bind(&server::validator, this));
        threadpool.create_thread(boost::bind(&server::validator, this));}
      if(do_block>0 && do_block<3){
        count_votes(now,cand);}
      if(do_block==3){
        std::cerr << "STOPing validation to finish block\n";
        do_validate=0;
        threadpool.join_all();
        std::cerr << "STOPed validation to finish block\n";
        finish_block();
        //writelastpath();
        writemsid();
        svid_.lock();
        svid_msgs_.clear();
        svid_.unlock();
        write_header(); // send new block signature
        do_block=0;
        do_validate=1;
        threadpool.create_thread(boost::bind(&server::validator, this));
        threadpool.create_thread(boost::bind(&server::validator, this));}
      boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
  }

  bool break_silence(uint32_t now,std::string& message) // will be obsolete if we start tolerating empty blocks
  { static uint32_t do_hallo=0;
    static uint32_t del=0;
#if BLOCKSEC == 0x20
    if(!do_block && do_hallo!=srvs_.now && now<srvs_.now+BLOCKSEC && now-srvs_.now>(uint32_t)(del) && svid_msgs_.size()<MIN_MSGNUM){
#else
    if(!do_block && do_hallo!=srvs_.now && now<srvs_.now+BLOCKSEC && now-srvs_.now>(uint32_t)(BLOCKSEC/4+opts_.svid*VOTE_DELAY) && svid_msgs_.size()<MIN_MSGNUM){
#endif
      std::cerr << "SILENCE, sending void message due to silence\n";
      usertxs txs(TXSTYPE_CON,opts_.port&0xFFFF,opts_.ipv4,0);
      message.append((char*)txs.data,txs.size);
      do_hallo=srvs_.now;
      del=rand()%BLOCKSEC;
      return(true);}
    if(do_hallo<srvs_.now-BLOCKSEC){
      do_hallo=srvs_.now-BLOCKSEC;
      del=rand()%BLOCKSEC;}
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
  { missing_.lock();
    missing_msgs_[msg->hash.num]=msg;
    missing_.unlock();
  }

  int check_msgs_size()
  { return(check_msgs_.size());
  }

//void lock_user(uint32_t cuser)
//{ ulock_[cuser & 0xff].lock();
//}

//void unlock_user(uint32_t cuser)
//{ ulock_[cuser & 0xff].unlock();
//}

  uint32_t srvs_now()
  { return(srvs_.now);
  }

  void join(peer_ptr participant);
  void leave(peer_ptr participant);
  void disconnect(uint16_t svid);
  const char* peers_list();
  bool connected(uint16_t svid);
  int duplicate(peer_ptr participant);
  void deliver(message_ptr msg);
  int deliver(message_ptr msg,uint16_t svid);
  void update(message_ptr msg);
  void svid_msid_rollback(message_ptr msg);
  void start_accept();
  void handle_accept(peer_ptr new_peer,const boost::system::error_code& error);
  void connect(std::string peer_address);
  void connect(uint16_t svid);
  void fillknown(message_ptr msg);
  void get_more_headers(uint32_t now);
  void ofip_gup_push(gup_t& g);
  void ofip_add_remote_deposit(uint32_t user,int64_t weight);
  void ofip_init(uint32_t myusers);
  void ofip_start();
  bool ofip_get_msg(uint32_t msid,std::string& line);
  void ofip_del_msg(uint32_t msid);
  void ofip_update_block(uint32_t period_start,uint32_t now,message_queue& commit_msgs,uint32_t newdiv);
  void ofip_process_log(uint32_t now);
  void ofip_add_remote_user(uint16_t abank,uint32_t auser,uint8_t* pkey);
  void ofip_delete_user(uint32_t user);

  //FIXME, move this to servers.hpp
  std::map<uint16_t,message_ptr> last_svid_msgs; // last validated message from server, should change this to now_svid_msgs
  std::map<uint16_t,msidhash_t> svid_msha; // copy of msid and hashed from last_svid_msgs
  //FIXME, use serv_.now instead
  servers last_srvs_;
  //message_ptr block; // my block message, now data in last_srvs_
  int do_sync;
  int do_fast;
  int do_check;
  //int get_headers; //TODO :-( reduce the number of flags
  uint32_t msid_; // change name to msid :-)
  int vip_max;
  boost::mutex peer_; //FIXME, make this private
  std::list<servers> headers; //FIXME, make this private
  uint32_t get_msglist; //block id of the requested msglist of messages
  office* ofip;
  uint8_t *pkey; //used by office/client to create BKY transaction
  uint32_t start_path;
  uint32_t start_msid;
private:
  hash_t skey;
  boost::mutex ulock_[0x100];
  enum { max_connections = 4 };
  enum { max_recent_msgs = 1024 }; // this is the block chain :->
  boost::asio::ip::tcp::endpoint endpoint_;
  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  boost::asio::ip::tcp::acceptor acceptor_;
  boost::thread* ioth_;
  servers srvs_;
  options& opts_;
  boost::thread_group threadpool;
  boost::thread* clock_thread;
  boost::thread* peers_thread;
  //uint8_t lasthash[SHA256_DIGEST_LENGTH]; // hash of last block, this should go to path/servers.txt
  //uint8_t prevhash[SHA256_DIGEST_LENGTH]; // hash of previous block, this should go to path/servers.txt
  int do_validate; // keep validation threads running
  //uint32_t maxnow; // do not process messages if time >= maxnow
  candidate_ptr winner; // elected candidate
  std::set<peer_ptr> peers_;
  std::map<hash_s,candidate_ptr,hash_cmp> candidates_; // list of candidates, TODO should be map of message_ptr
  message_queue wait_msgs_; //TODO, not used yet :-/
  message_queue check_msgs_;
  std::map<uint16_t,message_ptr> svid_msgs_; //last validated txs message or dbl message from server
  std::map<uint64_t,message_ptr> missing_msgs_; //TODO, start using this, these are messages we still wait for
  std::map<uint64_t,message_ptr> txs_msgs_; //_TXS messages (transactions)
  std::map<uint64_t,message_ptr> ldc_msgs_; //_TXS messages (transactions in sync mode)
  std::map<uint64_t,message_ptr> cnd_msgs_; //_CND messages (block candidates)
  std::map<uint64_t,message_ptr> blk_msgs_; //_BLK messages (blocks) or messages in a block when syncing
  std::map<uint64_t,message_ptr> dbl_msgs_; //_DBL messages (double spend)
  boost::mutex cand_;
  boost::mutex wait_;
  boost::mutex check_;
  boost::mutex svid_;
  boost::mutex missing_;
  boost::mutex txs_;
  boost::mutex ldc_;
  boost::mutex cnd_;
  boost::mutex blk_;
  boost::mutex dbl_;
  //boost::mutex gupstack_;
  // voting
  std::map<uint16_t,uint64_t> electors;
  uint64_t votes_max;
  int do_vote;
  int do_block;
  //int vip_ok;
  //int vip_no;
  //std::map<uint16_t,uint32_t> svid_msid; //TODO, maybe not used
  std::map<uint64_t,int64_t> deposit;
  boost::mutex deposit_;
  //std::map<uint64_t,hash_s> blk_bky; //change bank key
  std::map<uint64_t,std::vector<uint32_t>> blk_bnk; //create new bank
  std::map<uint64_t,std::vector<get_t>> blk_get; //set lock / withdraw
  std::map<uint64_t,std::vector<usr_t>> blk_usr; //remote account request
  std::map<uint64_t,std::vector<uok_t>> blk_uok; //remote account accept
  //std::map<uint64_t,std::vector<uint32_t>> blk_put; //cancel lock
  std::vector<int64_t> bank_fee;
  int64_t mydiv_fee; // just to record the local TXS_DIV_FEE bank income
  int64_t myusr_fee; // just to record the local TXS_DIV_FEE bank income
  int64_t myget_fee; // just to record the local TXS_DIV_FEE bank income
  uint32_t period_start; //start time of this period
};

#endif // SERVER_HPP
